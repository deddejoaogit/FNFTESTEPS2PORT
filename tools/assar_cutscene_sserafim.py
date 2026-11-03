#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Assa a CUTSCENE do `sserafim` num formato que o PS2 consegue desenhar.

O que a cena e' no original (medido nos arquivos, nao estimado):

  sserafim.hxc L567-930  `introCutscene()` -- uma sequencia de FlxTimers de
                         730/24 s que mexe camera, alpha, props, dois sons,
                         um texto de "Skip" e tres sprites de atlas Animate.
  SserafimCutsceneSprite  `loadTextureAtlas("cutscene/cutsceneMain")`
  SserafimGfSprite        `loadTextureAtlas("cutscene/gfGetUp")`
  SserafimBfSprite        `loadTextureAtlas("cutscene/bfGetUp")`

`Animation.json` do `cutsceneMain` tem **27 camadas**, **42 simbolos** e uma
timeline de **631 quadros a 24 fps** com simbolos ANINHADOS (o mesmo sprite
aparece varias vezes, em escalas diferentes, dentro de simbolos que tem
timeline propria). Duas saidas existem para isso no pipeline:

  1. rasterizar cada quadro (o que `tools/achatar_animate.py` faz para os
     personagens). Aqui NAO serve: 631 quadros de 1280x720 a 16 bpp seriam
     ~1,16 GB, e mesmo a 0,25 daria ~290 MB;
  2. assar a GEOMETRIA: por quadro, a lista de retangulos do spritemap e a
     matriz de cada um. E' o que este script faz. A cena inteira cabe em
     poucas centenas de KB de dados + UMA textura (o spritemap), e o desenho
     e' o mesmo que qualquer sprite do port ja' faz: quad + recorte + zoom.

Fidelidade: a matriz de cada elemento sai do `M3D` (4x4) do export -- e' o unico
campo que este documento traz. O decodificador de `MX` do
`tools/achatar_animate.py` NAO serve aqui: ele devolve a identidade quando nao
acha `MX`, e o resultado e' todo mundo desenhado em (0,0), sem erro nenhum.
Medido: com `TRP` (que e' pivo, nao translacao) sai um borrao; com `M3D` sai a
cena certa (BF e GF nos banquetos, /tmp/cut_m3d_0.png). O `--png-dir` escreve
quadros em PNG para conferencia a olho, e o `--conferir` compara o quadro assado
com o render direto do mesmo `M3D` (prova o empacotamento, nao o campo).

O que este script NAO faz (e o motivo esta' no plano):
  - o color grade do `SserafimShader` (brightness/hue/contrast/saturation) --
    e' shader por pixel; a cena e' assada sem ele;
  - o `FlxText` do "Skip" (o port desenha o texto com a fonte dele).

Uso:
  PYTHONPATH=/tmp/pylibs python3 tools/assar_cutscene_sserafim.py \\
      --arte /tmp/fassets --saida build/packs/stages/sserafim/cut --conferir 3
"""
import argparse
import json
import os
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import achatar_animate as aa  # noqa: E402  (Atlas, mat_apply, sprites)

# Os tres sprites da cena, com o nome do arquivo e o `Paths.image` do script.
# (sserafim.hxc L610-628 e os tres .hxc de preload/scripts/stages/props/)
SPRITES = [
    ("main", "cutscene/cutsceneMain", "SserafimCutsceneSprite"),
    ("gf", "cutscene/gfGetUp", "SserafimGfSprite"),
    ("bf", "cutscene/bfGetUp", "SserafimBfSprite"),
]

# O limite de largura de textura do port (PSMCT16) e' 1024; acima disso o
# `setup_texture_dims` recusa. A escala do atlas e' a MAIOR que cabe, como no
# empacotador de personagem.
MAX_LARGURA = 1024

# Formato do arquivo de quadros (FQC1):
#   cabecalho  16 B  "FQC1" | u16 n_sprites | u16 n_frames | u32 fps100 | u32 escala1000
#   sprites    8 B cada: u16 x, y, w, h   (no espaco do DOCUMENTO)
#   quadros    4 B cada: u32 offset (relativo ao fim da tabela de quadros)
#   quads      14 B cada: u16 sprite | i16 a,b,c,d (1/4096) | i16 tx,ty (1/4 px)
MAGIC = b"FQC1"
QUAD_BYTES = 14


def matriz(el):
    """Matriz (a,b,c,d,tx,ty) de um `SI`/`ASI`.

    Ordem por campo, nao por preferencia: `M3D` (4x4, a forma que este export
    usa) manda; `MX` (6 numeros) e' a forma antiga. Sem nenhum dos dois, `ASI`
    e' desenhado na identidade -- e isso fica CONTADO no manifesto, para o caso
    aparecer em vez de virar desenho no canto.
    """
    m3 = el.get("M3D")
    if m3 and len(m3) >= 16:
        return (float(m3[0]), float(m3[1]), float(m3[4]), float(m3[5]),
                float(m3[12]), float(m3[13]))
    mx = el.get("MX")
    if mx:
        return aa.mx_matrix(mx)
    sem_matriz.append(el.get("N") or el.get("SN") or "?")
    return aa.IDENT


def _i16(v, um):
    """Arredonda `v*um` para i16, com o limite do tipo (nunca estoura calado)."""
    n = int(round(v * um))
    if n > 32767:
        return 32767
    if n < -32768:
        return -32768
    return n


sem_matriz = []


def _emit(atlas, layers, parent, frame, inner, ops, depth):
    for lay, cur in atlas.layer_frames(layers, frame):
        if cur is None:
            continue
        fr, offset = cur
        for el in fr.get("E") or []:
            si = el.get("SI")
            if si is None:
                asi = el.get("ASI")
                if asi and asi.get("N") in atlas.sprites:
                    ops.append((aa.mat_mul(parent, matriz(asi)),
                                atlas.sprites[asi["N"]], asi.get("N")))
                continue
            nome = si.get("SN")
            world = aa.mat_mul(parent, matriz(si))
            if nome in atlas.symbols:
                # Simbolo aninhado: o quadro interno anda junto com o de fora
                # (`graphic`) ou comeca no `first` do pai -- as duas regras do
                # `inner_frame_for`, sem inventar uma terceira.
                sub = (atlas.symbols[nome].get("TL") or {}).get("L") or []
                _emit(atlas, sub, world,
                      aa.inner_frame_for(si, offset, inner, frame),
                      inner, ops, depth + 1)
            elif nome in atlas.sprites:
                ops.append((world, atlas.sprites[nome], nome))
    return ops


def quadra(atlas, frame):
    """-> [(matriz, sprite, nome)] na ordem de desenho do quadro."""
    inner = "graphic" if atlas.graphic else "first"
    return _emit(atlas, list(atlas.top.get("L") or []), aa.IDENT, frame,
                 inner, [], 0)


def assa(doc_atlas, nome, saida_dir, escala=None, bpp=16):
    """Assa UM atlas Animate. -> dict com o manifesto e o caminho da textura."""
    del sem_matriz[:]
    n_frames = max(1, doc_atlas.comprimento(doc_atlas.top))
    fps = float(doc_atlas.meta.get("FRT") or 24.0)

    # A textura: os spritemaps empacotados lado a lado nao existem aqui (o
    # export do sserafim tem UM spritemap). A escala maxima que cabe na
    # largura e' a escolhida, para nao perder nitidez sem motivo.
    sheets = {}
    for jf in sorted(__import__("glob").glob(os.path.join(doc_atlas.dir, "spritemap*.json"))):
        png = jf[:-5] + ".png"
        if os.path.isfile(png):
            sheets[jf] = Image.open(png).convert("RGBA")
    if len(sheets) != 1:
        raise SystemExit("%s: esperado 1 spritemap, achei %d" % (nome, len(sheets)))
    (jf, sheet), = sheets.items()
    if escala is None:
        escala = min(1.0, float(MAX_LARGURA) / float(sheet.size[0]))
    largura = max(1, int(round(sheet.size[0] * escala)))
    altura = max(1, int(round(sheet.size[1] * escala)))

    # A tabela de sprites, em coordenadas do DOCUMENTO (a escala vai no
    # cabecalho e o jogo multiplica na hora de desenhar).
    nomes, retangulos = [], []
    for n in sorted(doc_atlas.sprites):
        _folha, x, y, w, h = doc_atlas.sprites[n]
        if w <= 0 or h <= 0:
            continue
        nomes.append(n)
        retangulos.append((x, y, w, h))
    if not nomes:
        raise SystemExit("%s: spritemap sem retangulo" % nome)
    indice = {n: i for i, n in enumerate(nomes)}

    quadros, total_quads, vazios = [], 0, 0
    for f in range(n_frames):
        ops = quadra(doc_atlas, f)
        q = []
        for m, _sprite, n in ops:
            if n not in indice:
                continue
            a, b, c, d, tx, ty = m
            q.append((indice[n], _i16(a, 4096), _i16(b, 4096), _i16(c, 4096),
                      _i16(d, 4096), _i16(tx, 4), _i16(ty, 4)))
        if not q:
            vazios += 1
        quadros.append(q)
        total_quads += len(q)

    # Escreve os quadros.
    cab = MAGIC + struct.pack("<HHII", len(nomes), n_frames,
                              int(round(fps * 100)), int(round(escala * 1000)))
    cor = bytearray(cab)
    for x, y, w, h in retangulos:
        cor += struct.pack("<HHHH", x, y, w, h)
    tabela = bytearray()
    dados = bytearray()
    for q in quadros:
        tabela += struct.pack("<I", len(dados))
        for (i, a, b, c, d, tx, ty) in q:
            dados += struct.pack("<Hhhhhhh", i, a, b, c, d, tx, ty)
    quadros_bin = bytes(cor) + bytes(tabela) + bytes(dados)

    # A textura, num CFZ4 de UM quadro (o formato que o palco ja' le).
    pequena = sheet.resize((largura, altura), Image.NEAREST)
    folha_bytes = pequena.tobytes()

    os.makedirs(saida_dir, exist_ok=True)
    caminho_quads = os.path.join(saida_dir, "%s.quads" % nome)
    with open(caminho_quads, "wb") as fh:
        fh.write(quadros_bin)

    # A TEXTURA num CFZ4 de UM quadro: e' o formato que o palco ja' le'
    # (cabecalho, tabela de (offset,tamanho) e o bloco LZ4). O `_raster` do
    # achatador nao escreve arquivo -- quem escreve e' este trecho, com a mesma
    # funcao do empacotador de personagem (`to_ps2_bytes` + lz4).
    import lz4.block
    import pacote_personagem as pp
    caminho_tex = os.path.join(saida_dir, "%s.cfz4" % nome)
    bloco = lz4.block.compress(pp.to_ps2_bytes(pequena, bpp), store_size=False)
    dados_pack, tamanho = pp.write_pack([bloco], largura * altura * (bpp // 8))
    with open(caminho_tex, "wb") as fh:
        fh.write(dados_pack)

    return {
        "nome": nome,
        "quads": os.path.relpath(caminho_quads).replace(os.sep, "/"),
        "tex": os.path.relpath(caminho_tex).replace(os.sep, "/"),
        "quadros": n_frames,
        "fps": fps,
        "escala": round(escala, 4),
        "tex_w": largura,
        "tex_h": altura,
        "tex_bytes": largura * altura * (bpp // 8),
        "sprites": len(nomes),
        "quads_total": total_quads,
        "quads_por_quadro": round(float(total_quads) / n_frames, 2),
        "quadros_vazios": vazios,
        "bytes_quads": len(quadros_bin),
        "sem_matriz": len(sem_matriz),
    }


# --------------------------------------------------------------- conferencia --

def rasteriza_quads(doc_atlas, retangulos, nomes, quads):
    """Rasteriza os quads ASSADOS -- o jogo vai desenhar exatamente isto.

    E' a conferencia do empacotamento: matrizes em 1/4096 e deslocamentos em
    1/4 px, de volta ao espaco do documento.
    """
    ops = []
    for (i, a, b, c, d, tx, ty) in quads:
        m = (a / 4096.0, b / 4096.0, c / 4096.0, d / 4096.0, tx / 4.0, ty / 4.0)
        ops.append((m, doc_atlas.sprites[nomes[i]]))
    return aa.Atlas._raster(doc_atlas, ops, 1)


def escreve_pngs(doc_atlas, nome, png_dir, quantos, escala):
    """Escreve `quantos` quadros do atlas em PNG (conferencia a olho).

    O quadro sai no espaco do DOCUMENTO e na escala do atlas, que e' como o
    jogo vai desenha-lo. Fundo opaco escuro: silhueta preta em PNG transparente
    nao se ve' em visualizador nenhum.
    """
    os.makedirs(png_dir, exist_ok=True)
    n = max(1, doc_atlas.comprimento(doc_atlas.top))
    saida = []
    for k in range(quantos):
        f = int(round(k * (n - 1) / max(1, quantos - 1))) if quantos > 1 else 0
        ops = [(m, spr) for (m, spr, _n) in quadra(doc_atlas, f)]
        img, box = aa.Atlas._raster(doc_atlas, ops, 1)
        if img is None:
            continue
        fundo = Image.new("RGBA", img.size, (24, 24, 32, 255))
        fundo.alpha_composite(img)
        if escala != 1.0:
            fundo = fundo.resize((max(1, int(fundo.width * escala)),
                                  max(1, int(fundo.height * escala))),
                                 Image.NEAREST)
        caminho = os.path.join(png_dir, "%s_%03d.png" % (nome, f))
        fundo.convert("RGB").save(caminho)
        saida.append((f, caminho))
    return saida


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arte", default="/tmp/fassets",
                    help="clone de funkina.ssets (precisa de sserafim/images)")
    ap.add_argument("--saida", default="build/packs/stages/sserafim/cut")
    ap.add_argument("--escala", type=float, default=None,
                    help="escala do atlas (padrao: a maior que cabe em 1024)")
    ap.add_argument("--largura-max", type=int, default=1024,
                    help="largura maxima da textura (o GS recusa acima de 1024)")
    ap.add_argument("--conferir", type=int, default=0,
                    help="rasteriza N quadros e compara com a referencia")
    ap.add_argument("--png-dir", default=None,
                    help="escreve N quadros de cada sprite em PNG (conferencia)")
    ap.add_argument("--png-quantos", type=int, default=6)
    ap.add_argument("--manifesto", default=None,
                    help="caminho do JSON do manifesto (padrao: <saida>/manifest.json)")
    args = ap.parse_args()

    global MAX_LARGURA
    MAX_LARGURA = args.largura_max
    manifesto = {"bpp": 16, "sprites": [], "fonte": args.arte}
    for nome, chave, classe in SPRITES:
        atlas = aa.Atlas(args.arte, chave)
        info = assa(atlas, nome, args.saida, args.escala)
        info["chave"] = chave
        info["classe"] = classe
        manifesto["sprites"].append(info)
        print("%-5s %-26s %4d quadros @%g fps  atlas %dx%d = %d B"
              % (nome, chave, info["quadros"], info["fps"],
                 info["tex_w"], info["tex_h"], info["tex_bytes"]))
        print("      quadros assados: %d quads (%s por quadro), %d B%s"
              % (info["quads_total"], info["quads_por_quadro"], info["bytes_quads"],
                 "" if not info["quadros_vazios"] else
                 ", %d VAZIOS" % info["quadros_vazios"]))
        if info["sem_matriz"]:
            print("      ATENCAO: %d elemento(s) sem M3D nem MX (desenhados na "
                  "identidade)" % info["sem_matriz"])
        if args.png_dir:
            for f, caminho in escreve_pngs(atlas, nome, args.png_dir,
                                           args.png_quantos, info["escala"]):
                print("      png: quadro %4d -> %s" % (f, caminho))
        if args.conferir:
            path = os.path.join(args.saida, "%s.quads" % nome)
            largura = info["tex_w"]
            retangulos = []
            with open(path, "rb") as fh:
                d = fh.read()
            magic, nspr, nfr, fps100, esc1000 = struct.unpack("<4sHHII", d[:16])
            if magic != MAGIC:
                raise SystemExit("magic errado em %s" % path)
            # A tabela do arquivo guarda so' os retangulos; os NOMES saem na
            # mesma ordem em `assa` (o `sprites` do atlas, filtrado por
            # largura/altura). Reconstruir aqui e' o que permite rasterizar o
            # quadro assado com a folha certa.
            nomes = [n for n in sorted(atlas.sprites)
                     if atlas.sprites[n][3] > 0 and atlas.sprites[n][4] > 0]
            off = 16
            for i in range(nspr):
                retangulos.append(struct.unpack("<HHHH", d[off:off + 8]))
                off += 8
            base = off + 4 * nfr
            piores = []
            for f in range(0, nfr, max(1, nfr // max(1, args.conferir))):
                o = struct.unpack("<I", d[off + 4 * f:off + 4 * f + 4])[0]
                n = (struct.unpack("<I", d[off + 4 * (f + 1):off + 4 * (f + 1) + 4])[0]
                     if f + 1 < nfr else len(d) - base)
                quads = [struct.unpack("<Hhhhhhh", d[base + o + k * QUAD_BYTES:
                                                  base + o + (k + 1) * QUAD_BYTES])
                         for k in range((n - o) // QUAD_BYTES)]
                ref, _box = aa.Atlas._raster(
                    atlas, [(m, spr) for (m, spr, _n) in quadra(atlas, f)], 1)
                cand, _box2 = rasteriza_quads(atlas, retangulos, nomes, quads)
                iou = aa.score_against(ref, cand)
                piores.append((iou, f, len(quads)))
            piores.sort()
            print("      conferencia: %d quadros, pior IoU %.3f (quadro %d, %d quads)"
                  % (len(piores), piores[0][0], piores[0][1], piores[0][2]))

    destino = args.manifesto or os.path.join(args.saida, "manifest.json")
    with open(destino, "w", encoding="utf-8") as fh:
        json.dump(manifesto, fh, indent=1, sort_keys=True)
        fh.write("\n")
    print("manifesto: %s" % destino)


if __name__ == "__main__":
    main()
