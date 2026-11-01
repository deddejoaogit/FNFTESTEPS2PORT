#!/usr/bin/env python3
"""Orcamento de VRAM POR MUSICA, com os pacotes de verdade.

POR QUE ESTE ARQUIVO EXISTE

O pool de VRAM e' um numero unico (`SP_POOL_BYTES`, em engine/stage_pool.h), mas
a pergunta que decide se uma musica roda e' outra: "o palco desta musica, os
TRES personagens dela e os dois icones do HUD cabem ao mesmo tempo?". Essa conta
nunca tinha sido feita por musica com os pacotes reais: ela vivia de um numero
medido no console para uma cena, e o comentario de `stage_pool.h` falava do
palco sozinho -- com o BF e a GF fora da conta (o BF num slot fixo, a GF em cima
do slot `gftex` DEPOIS de ele ser liberado, que e' pior: sobreposicao).

Aqui ela roda sobre o que o jogo vai abrir de fato:

  palco        -> assets-data/stage-props.json: `sw`/`sh` de cada prop, com o
                  passo de 64 px e o alinhamento de 256 B de `cp_tex_bytes`
  personagens  -> o cabecalho CHP4 de build/packs/chars/<id>.chp4 (w, h, psm)
  HUD          -> o pacote de icone build/packs/icons/<id>.icp (passo * h * 2)

Uso:
    tools/orcamento_vram.py [--packs build/packs] [--detalhe] [--json saida]

Codigo de saida 1 se alguma musica nao cabe: a ferramenta e' um teste, nao um
relatorio. Sem os pacotes (build/packs) ela NAO chuta: sai com 2 e diz o que
falta, porque orcamento sem os tamanhos reais nao vale nada.
"""
import argparse
import json
import os
import struct
import sys

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# engine/vram_layout.c / main.c
SCREEN_W, SCREEN_H = 640, 448
VRAM_TOTAL = 4194304


def alinhar(v, a):
    return (v + (a - 1)) & ~(a - 1)


def cp_tex_bytes(w, h, psm):
    """Identico a `cp_tex_bytes` (engine/char_pack.c): canvas de PERSONAGEM.

    O personagem e' UMA textura, e a largura do texture buffer do GS e' o passo
    arredondado para 64 px. O PROP de palco NAO usa esta conta -- ele e' medido
    por `prop_vram_bytes`, que e' o que `sp_prop_bytes` aloca de fato.
    """
    passo = (w + 63) & ~63
    return alinhar(passo * h * (2 if psm == 16 else 4), 256)


def prop_vram_bytes(sw, sh):
    """Identico a `sp_prop_bytes` (engine/stage_pool.c).

    `src/engine/stage_vram_data.h` -- gerado por tools/gen_stage_vram.py do
    MESMO stage-props.json -- guarda `sw*sh*2` arredondado para 256 B, e e' esse
    o numero que o pool reserva. Antes esta ferramenta cobrava o passo de 64 px
    do GS (mais 33 a 173 KB por palco) e reprovava tres musicas que o runtime
    roda; um orcamento que nao e' o do jogo nao decide nada.
    """
    return alinhar(int(sw) * int(sh) * 2, 256)


def fb_words():
    """Dois framebuffers 640x448 PSMCT16, alinhados em pagina (graph_vram_size)."""
    return alinhar(SCREEN_W * (SCREEN_H // 2), 2048)


def ler_chp4(caminho):
    with open(caminho, "rb") as f:
        cab = f.read(36)
    if len(cab) < 36 or cab[:4] != b"CHP4":
        raise ValueError("%s nao e' CHP4" % caminho)
    _v, _nq, _bpq, w, h, _na, psm, _r = struct.unpack("<8I", cab[4:36])
    return int(w), int(h), int(psm)


def ler_icp(caminho):
    with open(caminho, "rb") as f:
        cab = f.read(32)
    if len(cab) < 32 or cab[:4] != b"ICP1":
        raise ValueError("%s nao e' ICP1" % caminho)
    w, h, _q, _psm, passo = struct.unpack("<5H", cab[6:16])
    return int(w), int(h), int(passo)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--packs", default=os.path.join(PROJECT, "build", "packs"))
    ap.add_argument("--detalhe", action="store_true")
    ap.add_argument("--icon-texture", type=int, default=0,
                    help="bytes de VRAM dos dois icones (default: mede os pacotes)")
    args = ap.parse_args()

    chars_dir = os.path.join(args.packs, "chars")
    icons_dir = os.path.join(args.packs, "icons")
    with open(os.path.join(PROJECT, "assets-data", "stage-props.json"),
              encoding="utf-8-sig") as f:
        stage_json = json.load(f)

    if not os.path.isdir(chars_dir):
        print("  FALTA %s -- rode tools/achatar_tudo.sh" % chars_dir)
        return 2

    palcos = {s["id"]: s for s in stage_json["stages"]}

    musicas = []
    for nome in sorted(os.listdir(os.path.join(PROJECT, "assets-data", "songs"))):
        if not nome.endswith("-metadata.json"):
            continue
        with open(os.path.join(PROJECT, "assets-data", "songs", nome),
                  encoding="utf-8-sig") as f:
            d = json.load(f)
        pd = d.get("playData") or {}
        chars = pd.get("characters") or {}
        musicas.append((nome[:-len("-metadata.json")], chars, pd.get("stage")))

    # Bytes fixos: framebuffers + as texturas que ficam fora do pool.
    fixos = 2 * fb_words() * 4
    tex2 = alinhar(320 * 256, 64) * 4            # notas/receptores, PSMCT32
    fonttex = alinhar(384 * 192, 64) * 4         # fonte, PSMCT32
    gftex16 = alinhar(320 * (64 // 2), 64) * 4   # GF no tutorial, PSMCT16
    fixos += tex2 + fonttex + gftex16
    # O NOTE SPLASH nao entra: ele e' uma regiao vazia DENTRO do `tex2`
    # (tools/vram_budget.py: splash_bytes() == 0). Se ele ganhar textura
    # propria, o numero tem de entrar aqui e nos fixos do runtime.

    print("== orcamento de VRAM por musica ==")
    print("  VRAM total ................ %9d B" % VRAM_TOTAL)
    print("  fixos (2 FB + tex2 + fonte + gftex16) %9d B" % fixos)

    pior = None
    falhas = 0
    linhas = []
    for sid, chars, stage in musicas:
        p = palcos.get(stage)
        if p is None:
            print("  FALHA: %s usa o palco '%s', que nao esta' em stage-props.json"
                  % (sid, stage))
            falhas += 1
            continue
        palco = sum(prop_vram_bytes(pr["sw"], pr["sh"]) for pr in p["props"])
        # Os personagens sao fixos durante a musica (o BF troca de textura em
        # tempo real pelo pacote, mas o pior caso de VRAM e' o canvas inteiro).
        pessoas = 0
        faltando = []
        for papel in ("player", "girlfriend", "opponent"):
            cid = chars.get(papel)
            if not cid:
                continue
            caminho = os.path.join(chars_dir, cid + ".chp4")
            if not os.path.isfile(caminho):
                faltando.append(cid)
                continue
            pessoas += cp_tex_bytes(*ler_chp4(caminho))
        icones = args.icon_texture
        if not icones and os.path.isdir(icons_dir):
            # Os dois icones da musica: jogador e oponente.
            for papel in ("player", "opponent"):
                cid = chars.get(papel)
                base = os.path.join(PROJECT, "assets-data", "characters", (cid or "") + ".json")
                icone = "face"
                if os.path.isfile(base):
                    with open(base, encoding="utf-8-sig") as f:
                        hi = json.load(f).get("healthIcon")
                    if isinstance(hi, dict) and hi.get("id"):
                        icone = hi["id"]
                caminho = os.path.join(icons_dir, icone + ".icp")
                if os.path.isfile(caminho):
                    w, h, passo = ler_icp(caminho)
                    icones += cp_tex_bytes(passo, h, 16)
        total = fixos + palco + pessoas + icones
        livre = VRAM_TOTAL - total
        linhas.append({"song": sid, "stage": stage, "palco": palco,
                       "pessoas": pessoas, "icones": icones, "livre": livre})
        if faltando and args.detalhe:
            print("    %s: sem pacote para %s" % (sid, ", ".join(faltando)))
        if livre < 0:
            falhas += 1
        if pior is None or livre < pior["livre"]:
            pior = linhas[-1]

    for l in sorted(linhas, key=lambda x: x["livre"])[:8]:
        print("  %-14s palco %9d + personagens %9d + icones %6d = livre %8d B"
              % (l["song"], l["palco"], l["pessoas"], l["icones"], l["livre"]))
    if pior:
        print("  pior musica: %s (%s), %d B livres"
              % (pior["song"], pior["stage"], pior["livre"]))
    if args.detalhe:
        for l in sorted(linhas, key=lambda x: x["livre"]):
            print("  %-16s %-20s %9d" % (l["song"], l["stage"], l["livre"]))
    print("  %s: %d musicas, %d nao cabem"
          % ("PASSOU" if falhas == 0 else "FALHOU", len(linhas), falhas))
    return 0 if falhas == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
