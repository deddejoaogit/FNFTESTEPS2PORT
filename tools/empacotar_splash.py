#!/usr/bin/env python3
"""Empacota a arte do NOTE SPLASH -- o brilho que aparece na nota SICK.

A REGRA, tirada da fonte v0.8.6
------------------------------
`PlayState.goodNoteHit` chama `playerStrumline.playNoteSplash(dir)` so' quando o
evento leva `doesNotesplash`, e o evento so' leva isso em SICK. Ou seja: splash
e' exclusivo de SICK e so' no strumline do JOGADOR.

`Strumline.playNoteSplash` (L1076-1098):

    splash.x = this.x + getXPos(direction) + INITIAL_OFFSET
             + noteStyle.getSplashOffsets()[0] * scale
    splash.y = this.y - INITIAL_OFFSET
             + noteStyle.getSplashOffsets()[1] * scale

`NoteSplash.play()` (L46-60) sorteia a animacao entre as `splash<DIR><n>`, poe
`frameRate = splashFramerate + random(-2, 2)` e faz
`offset.set(width * 0.3, height * 0.3)`; `onAnimationFinished` chama `kill()`.
O `offset` do flixel e' SUBTRAIDO no desenho, entao o canto superior esquerdo
cai em (x - 0.3w, y - 0.3h).

Constantes: `STRUMLINE_SIZE = 104` (L37), `INITIAL_OFFSET = -0.275 * 104 =
-28.6` (L45), `getXPos`: LEFT 0, DOWN 112, UP 224, RIGHT 336 (NOTE_SPACING =
112 = STRUMLINE_SIZE + 8).

O DADO (`preload/data/notestyles/funkin.json`, do repositorio de arte):

    assetPath "shared:noteSplashes", alpha 0.8, offsets [25, -5],
    leftSplashes  "note impact 1 purple0" / "note impact 2 purple0"
    downSplashes  "note impact 1  blue0"  / "note impact 2 blue0"   <- 2 espacos
    upSplashes    "note impact 1 green0"  / "note impact 2 green0"
    rightSplashes "note impact 1 red0"    / "note impact 2 red0"

`scale` NAO esta' no JSON: fica no padrao da classe, 1,0.

POR QUE A SPLASH MORA DENTRO DO ATLAS DO HUD
--------------------------------------------
Uma textura propria custaria VRAM que nao existe: o orcamento das 27 musicas
esta' apertado a 256 B em `stress`. Mas o atlas do HUD (`tex2`, 320x256
PSMCT32) e' usado so' nos primeiros 112 px -- notas em (lane*56, 0) 56x56 e a
barra de vida em (0,112) 301x10. A faixa de y=128 a 255 (320x128) esta' vazia,
e a splash cabe inteira ali. Custo de VRAM: ZERO. Custo de RAM: zero (o arquivo
e' lido direto para dentro de `g_hud_pixels`, que ja' existe, antes do upload
unico do HUD).

O que este arquivo escolhe e' o DIVISOR: a celula de cada animacao e' o tamanho
desenhado dividido por ele. A resolucao nao e' um gosto -- e' a MAIOR que cabe
na faixa livre, medida em passos de 0,25. A fidelidade da imagem e' negociavel
(regra do projeto); o evento, nao.

O QUE SAI
    build/packs/ui/splash.rgba      pixels (RGBA) da regiao, linha a linha
    assets-data/splash-plan.json    regiao, celulas, tempos, constantes
    src/engine/splash_dados.h       gerado por tools/gen_splash_dados.py

Uso:
    tools/empacotar_splash.py --assets <clone do funkin.assets>
    tools/empacotar_splash.py --ver
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "splash-plan.json")

# A regiao livre do atlas do HUD. As duas constantes que definem o limite de
# baixo do HUD estao repetidas aqui de proposito: se `tools/empacotar_ui.py`
# mudar a barra de vida de lugar, este arquivo tem de ser reavaliado, e o
# `--check` de tools/gen_splash_dados.py compara o plano com o header em C.
HUD_W, HUD_H = 320, 256
NOTAS_ATE_Y = 56          # lane*56 + 56 = 224 de largura, em y=0..56
BARRA_Y, BARRA_H = 112, 10
REGIAO_Y = 128            # primeira linha livre depois da barra de vida
REGIAO_H = HUD_H - REGIAO_Y

# As 8 animacoes na ordem em que o port as indexa: lane*2 + variante, com as
# lanes na ordem do note style (left, down, up, right).
ANIMACOES = [
    ("splashLEFT0",  "note impact 1 purple0"),
    ("splashLEFT1",  "note impact 2 purple0"),
    ("splashDOWN0",  "note impact 1  blue0"),
    ("splashDOWN1",  "note impact 2 blue0"),
    ("splashUP0",    "note impact 1 green0"),
    ("splashUP1",    "note impact 2 green0"),
    ("splashRIGHT0", "note impact 1 red0"),
    ("splashRIGHT1", "note impact 2 red0"),
]

QUADROS_POR_ANIM = 4
HALF = 0.5          # o HUD do port desenha em 640x360 = metade de 1280x720
CENTRO = 0.3        # NoteSplash.play: offset.set(width*0.3, height*0.3)
ALPHA = 0.8         # assets.noteSplash.alpha
OFF_X, OFF_Y = 25, -5
INITIAL_OFFSET = -0.275 * 104
FPS, FPS_VAR = 24, 2


def ler_xml(caminho):
    import xml.etree.ElementTree as ET
    raiz = ET.parse(caminho).getroot()
    quadros = {}
    for st in raiz:
        a = st.attrib
        nome = a.get("name")
        if not nome:
            continue
        quadros[nome] = {
            "x": int(a["x"]), "y": int(a["y"]),
            "w": int(a["width"]), "h": int(a["height"]),
            # frameX/frameY ausentes = 0: o quadro E' o canvas inteiro.
            "fx": int(a.get("frameX", 0)), "fy": int(a.get("frameY", 0)),
            "fw": int(a["frameWidth"]) if "frameWidth" in a else None,
            "fh": int(a["frameHeight"]) if "frameHeight" in a else None,
        }
    return quadros


def canvas_da_anim(quadros, ids):
    """(w, h) do canvas logico: o primeiro quadro que declara frameWidth."""
    for k in ids:
        if quadros[k]["fw"]:
            return quadros[k]["fw"], quadros[k]["fh"]
    q = quadros[ids[0]]
    return q["w"], q["h"]


def layout(divisor):
    """Celulas e regiao para um divisor. Devolve None se nao couber."""
    celulas = []
    x = 0
    for nome, prefixo in ANIMACOES:
        ids = ["%s%03d" % (prefixo, i) for i in range(QUADROS_POR_ANIM)]
        celulas.append({"nome": nome, "prefixo": prefixo, "ids": ids, "cx": x})
        # A largura vem do canvas; a altura da linha e' o maior canvas das 8
        # (a variante 2 e' mais baixa que a 1, entao a faixa e' uniforme).
        x += 0  # preenchido abaixo
    # Duas passadas: primeiro os tamanhos (que dependem do canvas), depois as
    # posicoes -- a largura da coluna depende do canvas da animacao.
    larg = []
    for c in celulas:
        lw, lh = c["lw"], c["lh"]
        c["cw"] = max(1, int(round(lw * HALF / divisor)))
        c["ch"] = max(1, int(round(lh * HALF / divisor)))
        larg.append(c["cw"])
    if sum(larg) > HUD_W:
        return None
    faixa = max(c["ch"] for c in celulas)
    if faixa * QUADROS_POR_ANIM > REGIAO_H:
        return None
    x = 0
    for c, w in zip(celulas, larg):
        c["cx"] = x
        x += w
    return celulas, x, faixa * QUADROS_POR_ANIM


def escolher_divisor(quadros):
    """O MENOR divisor (melhor resolucao) cujo atlas cabe na faixa livre."""
    canvas = {}
    for nome, prefixo in ANIMACOES:
        ids = ["%s%03d" % (prefixo, i) for i in range(QUADROS_POR_ANIM)]
        canvas[nome] = canvas_da_anim(quadros, ids)
    d = 1.0
    while d <= 12.0:
        celulas = []
        x = 0
        ok = True
        for nome, prefixo in ANIMACOES:
            lw, lh = canvas[nome]
            cw = max(1, int(round(lw * HALF / d)))
            ch = max(1, int(round(lh * HALF / d)))
            celulas.append({"nome": nome, "prefixo": prefixo, "lw": lw, "lh": lh,
                            "cw": cw, "ch": ch, "cx": x,
                            "ids": ["%s%03d" % (prefixo, i)
                                    for i in range(QUADROS_POR_ANIM)]})
            x += cw
            if x > HUD_W:
                ok = False
                break
        if ok:
            faixa = max(c["ch"] for c in celulas)
            if faixa * QUADROS_POR_ANIM <= REGIAO_H:
                return d, celulas, x, faixa * QUADROS_POR_ANIM
        d = round(d + 0.25, 2)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", help="clone do funkin.assets")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "ui"))
    ap.add_argument("--ver", action="store_true", help="so' mostra o plano")
    ap.add_argument("--check", action="store_true",
                    help="compara o atlas em --out com o plano commitado")
    args = ap.parse_args()

    if args.ver:
        d = json.load(open(PLANO, encoding="utf-8"))
        r = d["regiao"]
        print("regiao do HUD (%d,%d) %dx%d, divisor %.2f, %d B de VRAM NOVA"
              % (r["x"], r["y"], r["w"], r["h"], d["divisor"], 0))
        for a in d["animacoes"]:
            print("  %-12s %-22s celula %3d,%-3d %2dx%-2d  desenha %.1fx%.1f  %d q a %d fps"
                  % (a["nome"], a["prefixo"], a["cx"], a["cy"], a["cw"], a["ch"],
                     a["disp_w"], a["disp_h"], a["quadros"], a["fps"]))
        return 0

    if args.check:
        d = json.load(open(PLANO, encoding="utf-8"))
        r = d["regiao"]
        alvo = os.path.join(args.out, "splash.rgba")
        if not os.path.isfile(alvo):
            # Os pacotes de UI saem de tools/empacotar_*.py a partir da arte e
            # nao sao commitados (build/ e' ignorado). Sem o clone, o check do
            # atlas nao roda -- mas `tools/gen_splash_dados.py --check`, que e'
            # a porta de verdade (header x plano), roda sempre.
            print("  SKIP: %s nao existe (rode tools/empacotar_splash.py --assets ...)"
                  % os.path.relpath(alvo, PROJECT))
            return 0
        tam = os.path.getsize(alvo)
        quer = r["w"] * r["h"] * 4
        if tam != quer:
            print("  FALHA: %s tem %d B, o plano promete %d B (%dx%d)"
                  % (alvo, tam, quer, r["w"], r["h"]))
            return 1
        # A regiao nao pode invadir o que o HUD ja' desenha.
        if r["y"] + r["h"] > HUD_H or r["x"] + r["w"] > HUD_W:
            print("  FALHA: a regiao do splash sai do atlas do HUD")
            return 1
        if r["y"] < BARRA_Y + BARRA_H:
            print("  FALHA: a regiao do splash invade a barra de vida")
            return 1
        print("  ok:   splash.rgba %dx%d = %d B, dentro da faixa livre y=%d..%d"
              % (r["w"], r["h"], tam, r["y"], r["y"] + r["h"] - 1))
        return 0

    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets> (ou use --ver/--check)")
        return 1
    try:
        from PIL import Image
    except ImportError:
        print("  FALHA: falta o Pillow")
        return 1

    base = os.path.join(args.assets, "shared", "images")
    png = os.path.join(base, "noteSplashes.png")
    xml = os.path.join(base, "noteSplashes.xml")
    if not (os.path.isfile(png) and os.path.isfile(xml)):
        print("  FALTA %s (ou o .xml)" % png)
        return 1

    folha = Image.open(png).convert("RGBA")
    quadros = ler_xml(xml)

    escolha = escolher_divisor(quadros)
    if not escolha:
        print("  FALHA: nem o divisor 12 cabe em %dx%d" % (HUD_W, REGIAO_H))
        return 1
    divisor, celulas, largura, altura = escolha

    atlas = Image.new("RGBA", (largura, altura), (0, 0, 0, 0))
    faixa = altura // QUADROS_POR_ANIM
    for c in celulas:
        for i, k in enumerate(c["ids"]):
            if k not in quadros:
                print("  FALTA o quadro %r no noteSplashes.xml" % k)
                return 1
            q = quadros[k]
            # frameX/frameY deslocam o pedaco DENTRO do canvas logico: o canto
            # do pedaco cai em (-frameX, -frameY). Sem isso o brilho sai
            # deslocado dentro da propria celula.
            pedaco = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
            canvas = Image.new("RGBA", (c["lw"], c["lh"]), (0, 0, 0, 0))
            canvas.paste(pedaco, (-q["fx"], -q["fy"]))
            cel = canvas.resize((c["cw"], c["ch"]), Image.LANCZOS)
            # Centrada na faixa: as duas variantes tem alturas diferentes e sem
            # isso o brilho "pularia" entre quadros da mesma animacao.
            atlas.paste(cel, (c["cx"], i * faixa + (faixa - c["ch"]) // 2))

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "splash.rgba"), "wb") as f:
        f.write(atlas.tobytes())

    plano = {
        "nota": "Gerado por tools/empacotar_splash.py a partir de "
                "shared/images/noteSplashes.png+xml do funkin.assets. "
                "src/engine/splash_dados.h e o teste test_splash sao "
                "RENDERIZACOES deste arquivo.",
        "fonte": "shared/images/noteSplashes.png",
        "regiao": {"x": 0, "y": REGIAO_Y, "w": largura, "h": altura},
        "tex_w": HUD_W, "tex_h": HUD_H,
        "vram_nova": 0,
        "divisor": divisor,
        "faixa": faixa,
        "alpha": ALPHA,
        "offset_x": OFF_X, "offset_y": OFF_Y,
        "initial_offset": INITIAL_OFFSET,
        "centro": CENTRO,
        "fps": FPS, "fps_variancia": FPS_VAR,
        "animacoes": [{
            "nome": c["nome"], "prefixo": c["prefixo"],
            "cx": c["cx"], "cy": 0, "cw": c["cw"], "ch": c["ch"],
            # `lw`/`lh` sao o canvas LOGICO do original (1280x720). O tamanho
            # desenhado no port e' metade: o HUD desenha em 640x360.
            "lw": c["lw"], "lh": c["lh"],
            "disp_w": round(c["lw"] * HALF, 3), "disp_h": round(c["lh"] * HALF, 3),
            "quadros": QUADROS_POR_ANIM, "fps": FPS,
        } for c in celulas],
    }
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")

    print("divisor %.2f -> regiao (%d,%d) %dx%d  %d B (VRAM nova: 0)"
          % (divisor, plano["regiao"]["x"], plano["regiao"]["y"], largura, altura,
             largura * altura * 4))
    for a in plano["animacoes"]:
        print("  %-12s celula %3d,0 %2dx%-2d  desenha %.1fx%.1f"
              % (a["nome"], a["cx"], a["cw"], a["ch"], a["disp_w"], a["disp_h"]))
    print("plano escrito em assets-data/splash-plan.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
