#!/usr/bin/env python3
"""Empacota a ARTE DA TELA DE RESULTADO (v0.8.6, `ResultState.hx`).

O QUE A FONTE FAZ
-----------------
`ResultState.create()` (L119-160) monta a tela em 1280x720 com varias folhas
Sparrow e uma sequencia de `FlxTimer` (L359-400):

    resultsAnim   x = 1280 - 1480          y = -10    visivel em  6/24 s
                  animacao "result", prefixo "results instance 1", 24 fps
    ratingsPopin  x = -135                 y = 135    visivel em 21/24 s
                  animacao "idle", prefixo "Categories", 24 fps
    scorePopin    x = -180                 y = 515    visivel em 36/24 s
                  animacao "score", prefixo "tally score", 24 fps
    bg            gradiente vertical 0xFFFECC5C -> 0xFFFDC05C
                  (FlxGradient.createGradientFlxSprite(w, h, cores, 90))

`FullScreenScaleMode.gameCutoutSize.x` e' 0 em 16:9 sem entalhe -- e' o caso do
port, que desenha 16:9 em 640x360.

POR QUE SO' ESTAS TRES FOLHAS
-----------------------------
Sao as que montam a CENA da tela (fundo + as duas "popin"). As outras pecas do
`ResultState` (rankText, clearPercent, results-bf/pico, soundSystem, topBar,
highscoreNew) dependem de estado que o port ainda desenha como texto e ficam
registradas no docs/PLANO_COMPLETO.md com o tamanho medido de cada uma. Aqui
nao se inventa nada: cada folha empacotada tem linha no `ResultState.hx`.

ESCALA
------
As tres folhas sao desenhadas em 1280x720; o port desenha em 640x360, entao
tudo entra pela METADE -- e' a mesma regra que o HUD e o note splash seguem.
O canvas de cada animacao e' o `frameWidth/frameHeight` do XML e os quadros sao
compostos em (-frameX, -frameY), exatamente como o Sparrow do flixel faz.

FORMATO
-------
CFZ4 (o mesmo de `tools/pacote_personagem.py` e dos props de palco), lido no
console por `engine/pack_stream.c`: cabecalho de 16 B, tabela de (offset,
tamanho) por quadro e um bloco LZ4 independente por quadro. Assim o jogo le' UM
quadro por vez do pendrive e so' precisa da tabela na RAM.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_resultado.py --assets /tmp/fassets
    tools/empacotar_resultado.py --ver | --check
"""

import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")

# As tres folhas, com TUDO tirado do ResultState.hx (linha citada no topo).
# `x`/`y` sao em 1280x720, como na fonte; o port usa metade.
FOLHAS = [
    {
        "id": "results",
        "arquivo": "resultScreen/results",
        "prefixo": "results instance 1",
        "x": -200.0, "y": -10.0,          # 1280 - 1480
        "inicio_ms": round(1000.0 * 6 / 24),   # new FlxTimer().start(6 / 24)
        "fps": 24, "looped": False,
        "linha": "ResultState.hx L143 (posicao) e L359-367 (animacao/tempo)",
    },
    {
        "id": "ratings",
        "arquivo": "resultScreen/ratingsPopin",
        "prefixo": "Categories",
        "x": -135.0, "y": 135.0,
        "inicio_ms": round(1000.0 * 21 / 24),
        "fps": 24, "looped": False,
        "linha": "ResultState.hx L145 (posicao) e L369-377 (animacao/tempo)",
    },
    {
        "id": "score",
        "arquivo": "resultScreen/scorePopin",
        "prefixo": "tally score",
        "x": -180.0, "y": 515.0,
        "inicio_ms": round(1000.0 * 36 / 24),
        "fps": 24, "looped": False,
        "linha": "ResultState.hx L147 (posicao) e L379-388 (animacao/tempo)",
    },
]

# A ARTE DO RANK. `ResultState.displayRankText()` (L667-700) usa DUAS imagens do
# rank conquistado (Scoring.hx L627-666):
#
#   rankText<RNK>    vertical   FlxBackdrop vertical em x = 1280-44, y = 100,
#                               com velocity.y = -80 depois de 30/24 s, e um
#                               FlxFlicker de 0,25 s (2 quadros ligado, 2
#                               desligado) na entrada;
#   rankScroll<RNK>  horizontal 12 FlxBackdrop em x = 640-320, y = 60+67,5*i,
#                               metade com velocity.x = -7 e metade +7.
#
# Nomes dos arquivos por rank, como em Scoring.getVerTextAsset/getHorTextAsset:
# PERFECT e PERFECT_GOLD usam os mesmos de PERFECT.
RANKS = [
    # (indice no fnf_rank_t do port, sufixo do arquivo, atraso do flash em 1/24 s)
    (0, "LOSS", 186),       # SHIT
    (1, "GOOD", 107),
    (2, "GREAT", 109),
    (3, "EXCELLENT", 122),
    (4, "PERFECT", 129),    # PERFECT, e PERFECT_GOLD o reusa
    (5, "PERFECT", 129),    # PERFECT_GOLD
]

# O gradiente de fundo: FlxGradient com angulo 90 (vertical) e estas duas cores.
BG_TOPO = (0xFE, 0xCC, 0x5C)
BG_BASE = (0xFD, 0xC0, 0x5C)

HALF = 0.5
MAX_TEX = 1024


def ler_xml(caminho):
    import xml.etree.ElementTree as ET
    raiz = ET.parse(caminho).getroot()
    q = {}
    for st in raiz:
        a = st.attrib
        nome = a.get("name")
        if not nome:
            continue
        q[nome] = {
            "x": int(a["x"]), "y": int(a["y"]),
            "w": int(a["width"]), "h": int(a["height"]),
            "fx": int(a.get("frameX", 0)), "fy": int(a.get("frameY", 0)),
            "fw": int(a["frameWidth"]) if "frameWidth" in a else None,
            "fh": int(a["frameHeight"]) if "frameHeight" in a else None,
        }
    return q


def quadros_da_folha(quadros, prefixo):
    """Nomes dos quadros da animacao, em ordem -- como `addByPrefix`.

    `addByPrefix` casa o prefixo e ordena o resto pelo numero. O prefixo aqui
    NAO termina em digito (ex.: "Categories"), entao o numero comeca colado
    nele e o sufixo e' "0000", "0001", ...
    """
    achados = []
    for nome in quadros:
        if not nome.startswith(prefixo):
            continue
        resto = nome[len(prefixo):]
        if not resto or not resto.isdigit():
            continue
        achados.append((int(resto), nome))
    achados.sort()
    # `addByPrefix` numera a partir do primeiro que acha; a ordem e' pelo
    # numero, e numeros faltando nao existem nos XMLs do jogo.
    return [n for _i, n in achados]


def canvas(quadros, ids):
    for k in ids:
        if quadros[k]["fw"]:
            return quadros[k]["fw"], quadros[k]["fh"]
    q = quadros[ids[0]]
    return q["w"], q["h"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "resultado"))
    ap.add_argument("--ver", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.ver:
        d = json.load(open(PLANO, encoding="utf-8"))
        print("tela de resultado: %d folhas, fundo rgb(%02x%02x%02x)->rgb(%02x%02x%02x)"
              % (len(d["folhas"]), d["bg_topo"][0], d["bg_topo"][1], d["bg_topo"][2],
                 d["bg_base"][0], d["bg_base"][1], d["bg_base"][2]))
        for f in d["folhas"]:
            print("  %-8s %3d quadros  %3dx%-3d px  em (%6.1f,%6.1f)  aos %4d ms  %d fps"
                  % (f["id"], f["quadros"], f["w"], f["h"], f["x"], f["y"],
                     f["inicio_ms"], f["fps"]))
        return 0

    if args.check:
        d = json.load(open(PLANO, encoding="utf-8"))
        faltando = []
        for f in d["folhas"]:
            for parte in f["partes"]:
                if not os.path.isfile(os.path.join(args.out, parte)):
                    faltando.append(parte)
        if faltando:
            # Os pacotes saem de tools/empacotar_resultado.py a partir da arte e
            # nao sao commitados (build/ e' ignorado) -- sem eles o check dorme,
            # e quem e' porta de verdade e' tools/gen_resultado_dados.py --check.
            print("  SKIP: %d pacote(s) ausentes em %s (rode --assets ...)"
                  % (len(faltando), os.path.relpath(args.out, PROJECT)))
            return 0
        print("  ok:   %d folhas, %d pacote(s) presentes"
              % (len(d["folhas"]), sum(len(f["partes"]) for f in d["folhas"])))
        return 0

    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets> (ou --ver/--check)")
        return 1
    try:
        from PIL import Image
    except ImportError:
        print("  FALHA: falta o Pillow")
        return 1
    import lz4.block

    # write_pack/write_packs moram no empacotador de personagem: uma definicao
    # do CFZ4 no projeto, usada por todo mundo.
    sys.path.insert(0, HERE)
    import pacote_personagem as pp

    base = os.path.join(args.assets, "shared", "images")
    os.makedirs(args.out, exist_ok=True)
    folhas_out = []
    problemas = []

    for f in FOLHAS:
        png = os.path.join(base, f["arquivo"] + ".png")
        xml = os.path.join(base, f["arquivo"] + ".xml")
        if not (os.path.isfile(png) and os.path.isfile(xml)):
            problemas.append("%s: falta %s (ou o .xml)" % (f["id"], png))
            continue
        quadros = ler_xml(xml)
        ids = quadros_da_folha(quadros, f["prefixo"])
        if not ids:
            problemas.append("%s: nenhum quadro com o prefixo %r"
                             % (f["id"], f["prefixo"]))
            continue
        cw, ch = canvas(quadros, ids)
        w = max(1, int(round(cw * HALF)))
        h = max(1, int(round(ch * HALF)))
        if w > MAX_TEX or h > MAX_TEX:
            problemas.append("%s: %dx%d passa do limite de textura %d"
                             % (f["id"], w, h, MAX_TEX))
            continue
        folha = Image.open(png).convert("RGBA")
        blocos = []
        for k in ids:
            q = quadros[k]
            pedaco = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
            tela = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
            tela.paste(pedaco, (-q["fx"], -q["fy"]))
            if (w, h) != (cw, ch):
                tela = tela.resize((w, h), pp.Image.LANCZOS)
            blocos.append(lz4.block.compress(pp.to_ps2_bytes(tela, 32),
                                             store_size=False))
        caminho = os.path.join(args.out, f["id"] + ".cfz4")
        partes = pp.write_packs(blocos, w * h * 4, caminho)
        folhas_out.append({
            "id": f["id"],
            "prefixo": f["prefixo"],
            "fonte": f["arquivo"],
            "x": round(f["x"] * HALF, 3), "y": round(f["y"] * HALF, 3),
            "src_w": cw, "src_h": ch,
            "w": w, "h": h,
            "quadros": len(ids),
            "fps": f["fps"], "looped": f["looped"],
            "inicio_ms": f["inicio_ms"],
            "tex": "resultado/%s.cfz4" % f["id"],
            "partes": [os.path.basename(p[0]) for p in partes],
            "vram_bytes": w * h * 4,
            "linha": f["linha"],
        })

    if problemas:
        for p in problemas:
            print("  " + p)
        return 1

    # ---- A ARTE DO RANK: um par (vertical, horizontal) por rank ----
    # As imagens sao ESTATICAS (Paths.image, nao Sparrow), entao vao como RGBA
    # cru em vez de CFZ4: sem tabela, sem bloco, um arquivo por peca. A metade da
    # resolucao, como todo o resto da tela.
    ranks_out = []
    vistos = {}
    for idx, suf, flash in RANKS:
        if suf in vistos:
            # PERFECT_GOLD reusa o par de PERFECT (Scoring.hx L629-653).
            ranks_out.append(dict(vistos[suf], rank=idx))
            continue
        par = {}
        for tipo, pre in (("vert", "rankText"), ("hor", "rankScroll")):
            src = os.path.join(base, "resultScreen", "rankText", pre + suf + ".png")
            if not os.path.isfile(src):
                problemas.append("rank %s: falta %s" % (suf, src))
                continue
            im = Image.open(src).convert("RGBA")
            w = max(1, int(round(im.width * HALF)))
            h = max(1, int(round(im.height * HALF)))
            if w > MAX_TEX or h > MAX_TEX:
                problemas.append("rank %s/%s: %dx%d passa do limite %d"
                                 % (suf, tipo, w, h, MAX_TEX))
                continue
            im = im.resize((w, h), Image.LANCZOS)
            nome = "rank_%s_%s.rgba" % (suf.lower(), tipo)
            with open(os.path.join(args.out, nome), "wb") as f:
                f.write(im.tobytes())
            par[tipo] = {"arquivo": nome, "w": w, "h": h,
                         "vram_bytes": w * h * 4}
        if "vert" not in par or "hor" not in par:
            continue
        d = {
            "rank": idx, "sufixo": suf,
            "flash_ms": round(1000.0 * flash / 24.0),
            "vert": par["vert"], "hor": par["hor"],
            "vert_x": round((1280.0 - 44.0) * HALF, 3), "vert_y": round(100.0 * HALF, 3),
            "vert_vel": round(-80.0 * HALF, 3),
            "vert_espera_ms": round(1000.0 * 30.0 / 24.0),
            "hor_x": round((1280.0 / 2.0 - 320.0) * HALF, 3),
            "hor_y0": round((50.0 + 10.0) * HALF, 3),
            "hor_dy": round((135.0 / 2.0) * HALF, 3),
            "hor_n": 12,
            "hor_vel": round(7.0 * HALF, 3),
            "hor_espaco": round(10.0 * HALF, 1),
            "flicker_ms": round(1000.0 * 2.0 / 24.0),
            "flicker_total_ms": round(1000.0 * 6.0 / 24.0),
            "flash_dura_ms": round(1000.0 * 14.0 / 24.0),
            "linha": "ResultState.hx L667-700 + Scoring.hx L627-666",
        }
        vistos[suf] = d
        ranks_out.append(d)

    # O FUNDO: FlxGradient 90 graus de BG_TOPO a BG_BASE em 1280x720. No port ele
    # e' UMA textura estreita esticada na horizontal -- o gradiente so' varia em
    # Y, entao 16 colunas bastam e o custo de desenho e' um quad em vez de 360.
    # 16x360 RGBA = 23.040 B, dentro do pool que fica livre nesta tela.
    bgw, bgh = 16, 360
    bg = bytearray(bgw * bgh * 4)
    for y in range(bgh):
        t = y / float(bgh - 1)
        r = int(round(BG_TOPO[0] + (BG_BASE[0] - BG_TOPO[0]) * t))
        g = int(round(BG_TOPO[1] + (BG_BASE[1] - BG_TOPO[1]) * t))
        b = int(round(BG_TOPO[2] + (BG_BASE[2] - BG_TOPO[2]) * t))
        for x in range(bgw):
            o = (y * bgw + x) * 4
            bg[o + 0] = r; bg[o + 1] = g; bg[o + 2] = b; bg[o + 3] = 255
    with open(os.path.join(args.out, "bg.rgba"), "wb") as f:
        f.write(bytes(bg))

    plano = {
        "ranks": ranks_out,
        "bg_w": bgw, "bg_h": bgh, "bg_arquivo": "resultado/bg.rgba",
        "bg_vram_bytes": bgw * bgh * 4,
        "nota": "Gerado por tools/empacotar_resultado.py a partir de "
                "shared/images/resultScreen do funkin.assets. Constantes e "
                "posicoes: ResultState.hx v0.8.6. O header "
                "src/engine/resultado_dados.h e' RENDERIZACAO deste arquivo.",
        "escala": HALF,
        "bg_topo": list(BG_TOPO), "bg_base": list(BG_BASE),
        "folhas": folhas_out,
    }
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")

    total = sum(x["vram_bytes"] for x in folhas_out) + plano["bg_vram_bytes"]
    # O rank NAO entra nesse total: so' um par e' carregado por partida, e ele
    # e' medido a parte (`rank_vram_maior`).
    rank_max = max([r["vert"]["vram_bytes"] + r["hor"]["vram_bytes"]
                    for r in ranks_out] or [0])
    plano["rank_vram_maior"] = rank_max
    print("tela de resultado: %d folhas + fundo %dx%d, %d B de VRAM no total"
          % (len(folhas_out), bgw, bgh, total))
    for f in folhas_out:
        print("  %-8s %3d quadros  %3dx%-3d  em (%6.1f,%6.1f) aos %4d ms  "
              "%d B  %d pacote(s)"
              % (f["id"], f["quadros"], f["w"], f["h"], f["x"], f["y"],
                 f["inicio_ms"], f["vram_bytes"], len(f["partes"])))
    print("rank: %d pares, o maior custa %d B de VRAM (um par por partida)"
          % (len(ranks_out), rank_max))
    print("plano escrito em assets-data/resultado-plan.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
