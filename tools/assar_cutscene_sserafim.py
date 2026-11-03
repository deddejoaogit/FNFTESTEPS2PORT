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
# Os tres numeros de posicao/scroll sao os do `createCutsceneSprites`
# (L610-628: `setPosition` + `scrollFactor.set`) e servem a CONFERENCIA de tela
# do fim deste arquivo: a cena tem de cair DENTRO do quadro.
SPRITES = [
    ("main", "cutscene/cutsceneMain", "SserafimCutsceneSprite", -395.0, 10.0, 0.94),
    ("gf", "cutscene/gfGetUp", "SserafimGfSprite", 655.0, -104.0, 0.95),
    ("bf", "cutscene/bfGetUp", "SserafimBfSprite", 1220.0, 531.0, 0.99),
]

# A LARGURA DO ATLAS: 512 px, e nao o limite de 1024 do port. Nao e' preferencia:
# o pool do palco (`stage_pool.h`, 2.383.872 B) tem de segurar as TRES texturas
# da cena JUNTO com os props que ela mostra. Medido no harness com a arte real,
# pela menor folga do pool durante a cena (`cut_pool_min`, ver main.c):
#
#   atlas 448 px -> 591.360 B de textura -> 649.152 B livres no pior instante
#   atlas 512 px -> 773.120 B            -> 311.296 B livres   <- escolhido
#   atlas 640 px -> 1.207.040 B          ->  33.792 B livres   (1,4% do pool)
#
# 640 cabe, mas com 1,4% de folga qualquer prop a mais no instante seguinte
# despeja outro -- e despejo e' reenvio de textura por DMA. 512 custa 1,44x de
# ampliacao na tela (a cena roda com zoom 0,7 e o documento tem ~2100 px de
# largura, que viram ~736 px de tela) e deixa 13% do pool livre.
MAX_LARGURA = 512

# O formato (FQC1 v2) mora em tools/cutscene_pacote.py, e o MESMO escritor
# serve o gerador de assets sinteticos do harness -- assim o harness exercita o
# formato que o console le', em vez de um parecido.
import cutscene_pacote as cp  # noqa: E402


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


# As duas cameras que a cena usa, em unidades do DOCUMENTO (1280x720):
#   t=3..563/24  zoom 0,7  centro (660,300)     -- a cena inteira
#   t=650/24..   zoom 0,55 centro (1070,470)    -- a volta para o palco
# (sserafim.hxc L673-677, L795-806). A conta de tela e' a MESMA de
# `src/engine/stage_layout.c`: 320 + (mundo - cam*scroll + 640*scroll - 640)*0,5*zoom.
CAMERAS_CENA = [((660.0, 300.0), 0.7), ((1070.0, 470.0), 0.55)]
TELA_W, TELA_H = 640.0, 360.0


def confere_tela(quadros, retangulos, pos, scroll):
    """Fracao dos quadros que aparecem na tela em pelo menos UMA das cameras.

    ESTA CONFERENCIA EXISTE POR UM DEFEITO REAL: o pacote guardava as matrizes
    no espaco do SPRITEMAP e o `FlxAnimate` desenha a partir do canto do
    simbolo (`matrix.translate(-bounds.x, -bounds.y)`), entao a `gfGetUp`,
    colocada em (655,-104), saia em y -644..-41 do documento -- acima do topo.
    A cena inteira estava deslocada e nenhum teste pegava: o pacote era valido,
    o desenho nao errava o quadro, e o resultado era tela vazia. Agora um
    atlas que nao cai no quadro derruba o gerador.
    """
    dentro = 0
    for q in quadros:
        ok = False
        for (cx, cy), zoom in CAMERAS_CENA:
            sc = 0.5 * zoom
            for (i, a, b, c, d, tx, ty) in q:
                w, h = retangulos[i][2], retangulos[i][3]
                for lx, ly in ((0.0, 0.0), (w, 0.0), (w, h), (0.0, h)):
                    x = pos[0] + tx / 4.0 + (a / 4096.0) * lx + (b / 4096.0) * ly
                    y = pos[1] + ty / 4.0 + (c / 4096.0) * lx + (d / 4096.0) * ly
                    sx = 320.0 + (x - cx * scroll + 640.0 * scroll - 640.0) * sc
                    sy = 180.0 + (y - cy * scroll + 360.0 * scroll - 360.0) * sc
                    if 0.0 <= sx <= TELA_W and 0.0 <= sy <= TELA_H:
                        ok = True
                        break
                if ok:
                    break
            if ok:
                break
        if ok:
            dentro += 1
    return float(dentro) / float(max(1, len(quadros)))


def assa(doc_atlas, nome, saida_dir, escala=None, bpp=16, pos=(0.0, 0.0),
         scroll=1.0):
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

    # A ORIGEM DO SIMBOLO, que e' o que faltava para a cena cair no lugar.
    # O `FlxAnimate` NAO desenha a linha do tempo a partir de (0,0): ele faz
    # `matrix.translate(-bounds.x, -bounds.y)` com `bounds = timeline._bounds`
    # (`FlxAnimate.hx` L214, e o `_bounds` e' `getWholeBounds()` em
    # `Timeline.hx` L367, ou seja a UNIAO dos quadros do simbolo inteiro).
    # Sem isso cada sprite da cena era desenhado com o canto do SPRITEMAP na
    # posicao dele e o conteudo saia fora da tela -- medido nos tres atlas
    # reais: a `gfGetUp`, colocada em (655,-104), aparecia em y -644..-41, ou
    # seja ACIMA do topo do documento, e a cena inteira ficava deslocada.
    # Por isso as translacoes do pacote saem RELATIVAS a esse canto: o jogo
    # soma a posicao do sprite e nada mais (`ssc_cantos`), que e' a conta do
    # original. A origem vai no manifesto, para o numero nao virar folclore.
    quadros, total_quads, vazios = [], 0, 0
    bx = by = None
    for f in range(n_frames):
        ops = quadra(doc_atlas, f)
        q = []
        for m, _sprite, n in ops:
            if n not in indice:
                continue
            a, b, c, d, tx, ty = m
            w, h = retangulos[indice[n]][2], retangulos[indice[n]][3]
            for lx, ly in ((0.0, 0.0), (w, 0.0), (0.0, h), (w, h)):
                cx = tx + a * lx + b * ly
                cy = ty + c * lx + d * ly
                if bx is None or cx < bx:
                    bx = cx
                if by is None or cy < by:
                    by = cy
            q.append((indice[n], a, b, c, d, tx, ty))
        quadros.append(q)
    if bx is None:
        bx = by = 0.0
    # Segunda passada: desconta o canto e arredonda para o formato.
    finais = []
    for q in quadros:
        fq = []
        for (i, a, b, c, d, tx, ty) in q:
            fq.append((i, _i16(a, 4096), _i16(b, 4096), _i16(c, 4096),
                       _i16(d, 4096), _i16(tx - bx, 4), _i16(ty - by, 4)))
        if not fq:
            vazios += 1
        total_quads += len(fq)
        finais.append(fq)
    quadros = finais

    # As ETIQUETAS de quadro do Animate: e' assim que o `resetAnim()` e o
    # `doAnim()` do `SserafimGfSprite` escolhem entre `static` e `getup`. Sem
    # elas o port so' teria a linha do tempo inteira, e a "get up" comecaria a
    # se mexer 60 quadros antes da hora.
    # O NOME do rotulo tem 8 bytes no pacote, porque e' por ele que o desenho o
    # procura ("static", "getup"). Um rotulo mais longo NAO e' truncado -- seria
    # outro nome -- e tambem nao derruba o pacote: ele e' o nome do simbolo
    # inteiro (o `cutsceneMain`, que o `doAnim()` toca com `play("")`), e esse
    # caminho nao passa por rotulo nenhum. Os pulados sao REPORTADOS.
    rotulos, pulados = [], []
    for nome_anim, (primeiro, quantos) in sorted(
            doc_atlas.animations().items()):
        if len(nome_anim.encode("utf-8")) > cp.NOME_BYTES:
            pulados.append(nome_anim)
            continue
        rotulos.append((nome_anim, primeiro, quantos))
    if pulados:
        print("      %d rotulo(s) com nome maior que %d bytes, fora da tabela: %s"
              % (len(pulados), cp.NOME_BYTES, ", ".join(repr(x) for x in pulados)))
    if len(rotulos) > cp.MAX_LABELS:
        raise SystemExit("%s: %d rotulos, o leitor aceita %d"
                         % (nome, len(rotulos), cp.MAX_LABELS))

    cobertura = confere_tela(quadros, retangulos, pos, scroll)
    if cobertura < 0.5:
        raise SystemExit(
            "%s: %.0f%% dos quadros caem FORA da tela com a camera da cena -- "
            "o pacote esta' com a origem do simbolo errada (ver confere_tela)"
            % (nome, 100.0 * (1.0 - cobertura)))

    # Escreve os quadros.
    quadros_bin = cp.monta_pacote(nomes, retangulos, quadros, fps, escala,
                                  largura, altura, rotulos)

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
        # O canto do simbolo que as translacoes do pacote JA' descontam
        # (`timeline._bounds` do `FlxAnimate`), em px do DOCUMENTO.
        "origem_simbolo": [round(bx, 2), round(by, 2)],
        "cobertura_tela": round(cobertura, 4),
        "rotulos": {n: [p0, q] for (n, p0, q) in rotulos},
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


# O FIM DA MUSICA (`endStuff`, sserafim.hxc L680-706): duas imagens soltas,
# `end/end1` e `end/end2`, sem animacao nenhuma -- por isso nao passam pelo
# assador de geometria. Elas vao para o MESMO pacote CFZ4 de 1 quadro que os
# props usam e sao desenhadas em coordenadas de `camCutscene`, que cobre o
# documento inteiro (1280x720) -> a escala do port e' 0,5 (`escala1000` 500).
#
# Medidas na arte: end1 753x752, end2 1080x75. O script desenha as duas com
# `scale 0.67` e `updateHitbox()`, entao o tamanho NA TELA e' o da textura do
# pacote dividido por 2 (o documento) -- o texto vai para o manifesto para o
# desenho do `main.c` e o teste do PC usarem o mesmo numero.
FIM_IMAGENS = [
    # (arquivo, nome de saida, largura em px do pacote)
    ("end/end1", "end1", 377),
    ("end/end2", "end2", 540),
]


def assa_fim(arte, saida_dir, bpp=16):
    """As duas imagens do fim da musica -> [(nome, w, h, bytes, caminho)]."""
    import lz4.block
    import pacote_personagem as pp
    out = []
    for chave, nome, largura in FIM_IMAGENS:
        caminho_png = os.path.join(arte, "sserafim", "images", chave + ".png")
        if not os.path.isfile(caminho_png):
            raise SystemExit("%s: falta %s" % (nome, caminho_png))
        im = Image.open(caminho_png).convert("RGBA")
        alt = max(1, int(round(im.size[1] * float(largura) / float(im.size[0]))))
        im = im.resize((largura, alt), Image.LANCZOS)
        os.makedirs(saida_dir, exist_ok=True)
        bloco = lz4.block.compress(pp.to_ps2_bytes(im, bpp), store_size=False)
        quadros_bytes = largura * alt * (bpp // 8)
        dados, _tamanho = pp.write_pack([bloco], quadros_bytes)
        destino = os.path.join(saida_dir, nome + ".cfz4")
        with open(destino, "wb") as fh:
            fh.write(dados)
        # `tex_bytes` e' o que a VRAM ocupa (um quadro descomprimido), nao o
        # tamanho do arquivo: e' com ele que o pool e o orcamento contam.
        out.append({"nome": nome, "tex_w": largura, "tex_h": alt,
                    "tex_bytes": quadros_bytes, "arquivo_bytes": len(dados),
                    "tex": os.path.relpath(destino).replace(os.sep, "/")})
    return out


def escreve_header_fim(itens, destino):
    """Escreve `src/engine/cut_fim_dados.h` com o TAMANHO das duas imagens.

    O pacote CFZ4 nao guarda a largura e a altura (so' o `frame_bytes`, que e'
    `w*h*2`) e as duas imagens do fim nao tem `.quads` onde caiba o cabecalho.
    Em vez de escrever 377/376/540/38 a mao no `main.c` -- que seria um numero
    inventado na primeira vez que a arte mudasse --, o gerador publica o header,
    do mesmo jeito que `gen_stage_criados.py` publica `stage_criados_dados.h`.
    O `main.c` CONFERE `frame_bytes == w*h*2` antes de desenhar: header e pacote
    de versoes diferentes viram falha reportada, nao imagem torta.
    """
    linhas = [
        "/* GERADO por tools/assar_cutscene_sserafim.py -- NAO EDITAR A MAO.",
        " *",
        " * Tamanho, em pixels, das duas imagens do fim da musica do `sserafim`",
        " * (`endStuff`, sserafim.hxc L680-706). O `.cfz4` nao guarda largura nem",
        " * altura; o header e' a unica fonte desses numeros para o `main.c`. */",
        "#ifndef FNF_CUT_FIM_DADOS_H",
        "#define FNF_CUT_FIM_DADOS_H",
        "",
        "#define CUT_FIM_IMAGENS %d" % len(itens),
        "static const int cut_fim_tex_w[CUT_FIM_IMAGENS] = {%s};"
        % ", ".join(str(i["tex_w"]) for i in itens),
        "static const int cut_fim_tex_h[CUT_FIM_IMAGENS] = {%s};"
        % ", ".join(str(i["tex_h"]) for i in itens),
        "",
        "#endif /* FNF_CUT_FIM_DADOS_H */",
        "",
    ]
    with open(destino, "w", encoding="utf-8") as fh:
        fh.write("\n".join(linhas))


def main():
    # `global` ANTES de qualquer leitura da constante: o valor padrao do
    # `--largura-max` e' justamente o MAX_LARGURA do modulo, e declarar o
    # `global` depois disso e' erro de sintaxe no Python (`name is used prior to
    # global declaration`).
    global MAX_LARGURA
    ap = argparse.ArgumentParser()
    ap.add_argument("--arte", default="/tmp/fassets",
                    help="clone de funkina.ssets (precisa de sserafim/images)")
    ap.add_argument("--saida", default="build/packs/stages/sserafim/cut")
    ap.add_argument("--escala", type=float, default=None,
                    help="escala do atlas (padrao: a maior que cabe em --largura-max)")
    ap.add_argument("--largura-max", type=int, default=MAX_LARGURA,
                    help="largura do atlas em px (padrao 512 -- ver MAX_LARGURA; "
                         "o GS recusa acima de 1024)")
    ap.add_argument("--conferir", type=int, default=0,
                    help="rasteriza N quadros e compara com a referencia")
    ap.add_argument("--png-dir", default=None,
                    help="escreve N quadros de cada sprite em PNG (conferencia)")
    ap.add_argument("--png-quantos", type=int, default=6)
    ap.add_argument("--header", default=None,
                    help="header em C com o tamanho das imagens do fim "
                         "(padrao: src/engine/cut_fim_dados.h)")
    ap.add_argument("--manifesto", default=None,
                    help="caminho do JSON do manifesto (padrao: <saida>/manifest.json)")
    args = ap.parse_args()

    MAX_LARGURA = args.largura_max
    manifesto = {"bpp": 16, "sprites": [], "fonte": args.arte}
    for nome, chave, classe, px, py, scr in SPRITES:
        atlas = aa.Atlas(args.arte, chave)
        info = assa(atlas, nome, args.saida, args.escala, pos=(px, py), scroll=scr)
        info["pos"] = [px, py]
        info["scroll"] = scr
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
            (magic, nspr, nfr, fps100, esc1000, texw, texh, nlab,
             _res) = struct.unpack("<4sHHIIHHHH", d[:24])
            if magic != cp.MAGIC:
                raise SystemExit("magic errado em %s" % path)
            if (texw, texh) != (info["tex_w"], info["tex_h"]) or \
               nlab != len(info["rotulos"]):
                raise SystemExit("cabecalho incoerente em %s" % path)
            # A tabela do arquivo guarda so' os retangulos; os NOMES saem na
            # mesma ordem em `assa` (o `sprites` do atlas, filtrado por
            # largura/altura). Reconstruir aqui e' o que permite rasterizar o
            # quadro assado com a folha certa.
            nomes = [n for n in sorted(atlas.sprites)
                     if atlas.sprites[n][3] > 0 and atlas.sprites[n][4] > 0]
            off = 24
            for i in range(nspr):
                retangulos.append(struct.unpack("<HHHH", d[off:off + 8]))
                off += 8
            off += 12 * nlab
            base = off + 4 * nfr
            piores = []
            for f in range(0, nfr, max(1, nfr // max(1, args.conferir))):
                o = struct.unpack("<I", d[off + 4 * f:off + 4 * f + 4])[0]
                n = (struct.unpack("<I", d[off + 4 * (f + 1):off + 4 * (f + 1) + 4])[0]
                     if f + 1 < nfr else len(d) - base)
                quads = [struct.unpack("<Hhhhhhh", d[base + o + k * cp.QUAD_BYTES:
                                                  base + o + (k + 1) * cp.QUAD_BYTES])
                         for k in range((n - o) // cp.QUAD_BYTES)]
                ref, _box = aa.Atlas._raster(
                    atlas, [(m, spr) for (m, spr, _n) in quadra(atlas, f)], 1)
                cand, _box2 = rasteriza_quads(atlas, retangulos, nomes, quads)
                iou = aa.score_against(ref, cand)
                piores.append((iou, f, len(quads)))
            piores.sort()
            print("      conferencia: %d quadros, pior IoU %.3f (quadro %d, %d quads)"
                  % (len(piores), piores[0][0], piores[0][1], piores[0][2]))

    # As duas imagens do FIM DA MUSICA, no mesmo diretorio e no mesmo manifesto:
    # `end1` no centro da tela (0,05 s depois do evento) e `end2` no canto
    # inferior direito, 40 px do documento de margem (L693-700).
    manifesto["fim"] = assa_fim(args.arte, args.saida)
    for it in manifesto["fim"]:
        print("%-6s %-28s %4dx%-4d = %d B" % (it["nome"], "end/" + it["nome"],
                                               it["tex_w"], it["tex_h"], it["tex_bytes"]))
    raiz_projeto = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    escreve_header_fim(manifesto["fim"],
                       args.header or os.path.join(raiz_projeto, "src", "engine",
                                                   "cut_fim_dados.h"))
    destino = args.manifesto or os.path.join(args.saida, "manifest.json")
    with open(destino, "w", encoding="utf-8") as fh:
        json.dump(manifesto, fh, indent=1, sort_keys=True)
        fh.write("\n")
    print("manifesto: %s" % destino)


if __name__ == "__main__":
    main()
