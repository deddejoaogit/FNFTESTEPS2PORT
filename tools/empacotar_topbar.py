#!/usr/bin/env python3
"""Empacota a BARRA DO TOPO da tela de Resultado: a barra preta e as imagens de
dificuldade.

A BARRA NAO TEM ARTE (`BitmapUtil.createResultsBar`, v0.8.6)
------------------------------------------------------------
O original monta a barra em codigo, sem PNG:

    final width = Math.ceil(FlxG.width * 1.011);      // 1295
    final mainBitmap = new BitmapData(width, Math.ceil(width / 8.7), ...);
                                                      // 1295 x 149, PRETO
    matrix.rotate(-3.8 * Math.PI / 180);
    matrix.translate(-15, 0);
    rect.width -= 15;                                 // recorte de 1280
    bitmap.draw(mainBitmap, matrix, rect, true);

Ou seja: um retangulo preto girado de -3,8 graus e deslocado 15 px para a
esquerda, recortado em (0, 0, 1280, 149). O port nao tem rotacao de quad, mas a
forma resultante e' simples: a barra cobre TUDO acima de uma reta inclinada
(a borda de baixo). Este empacotador calcula essa reta com a matriz do original
e a deixa no plano; o jogo desenha a barra em faixas, sem textura (0 B de VRAM).

A IMAGEM DA DIFICULDADE (`diff_<id>.png`)
------------------------------------------
`ResultState.hx` L332-334 carrega `resultScreen/diff_${difficultyId}` e a poe em
x = 555; `timerThenSongName()` (L736-758) faz a entrada: y de `-height` ate'
`122 + (bar.height - 148)` em 0,5 s com `FlxEase.expoOut` e 0,8 s de atraso.
Sao cinco arquivos e o jogo carrega UM por partida -- o da dificuldade jogada.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_topbar.py --assets /tmp/fassets
"""

import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")

HALF = 0.5
MAX_TEX = 1024

# Os cinco arquivos de dificuldade (ResultState.hx L332 usa o id da dificuldade).
DIFICULDADES = ["easy", "normal", "hard", "erect", "nightmare"]


def geometria_da_barra():
    """A borda de baixo da barra preta, em 640x360, e o tamanho da barra.

    Refaz a conta de `BitmapUtil.createResultsBar`:
        dest = R(-3,8 graus) * src + (-15, 0)
    e devolve, para a tela de 640x360, a altura da borda em x = 0 e em x = 640.
    (A borda de cima fica acima da tela nos dois extremos, entao a barra e'
    "tudo acima da reta de baixo".)
    """
    largura_src = int(math.ceil(1280 * 1.011))          # 1295
    altura_src = int(math.ceil(largura_src / 8.7))      # 149
    ang = math.radians(-3.8)
    c, s = math.cos(ang), math.sin(ang)

    def destino(x, y):
        # Matrix.rotate + translate do flixel (y para baixo).
        return (c * x - s * y - 15.0, s * x + c * y)

    # A borda de baixo e' a reta src y = altura_src.
    xa, ya = destino(0.0, float(altura_src))
    xb, yb = destino(float(largura_src), float(altura_src))
    # Em coordenadas da TELA (metade), a reta vai de x=0 a x=640.
    def y_em(x_half):
        x_src = x_half / HALF
        t = (x_src - xa) / (xb - xa)
        return (ya + (yb - ya) * t) * HALF

    return {
        "largura_src": largura_src,
        "altura_src": altura_src,
        "borda_y0": round(y_em(0.0), 3),
        "borda_y1": round(y_em(640.0), 3),
        "angulo": -3.8,
        "translate_x": -15,
        # y do sprite da barra: tween de -altura ate' 0 em 7/24 s, quartOut,
        # com 3/24 s de atraso (ResultState.hx L342-344).
        "altura_half": round(altura_src * HALF, 3),
        "atraso_q": 3,
        "tween_q": 7,
        # (blackTopBar.height - 148): a correcao que a fonte aplica em tudo que
        # fica abaixo da barra (dificuldade, % pequeno, nome da musica).
        "correcao_y": round((altura_src - 148.0) * HALF, 3),
    }


def empacotar(assets, out):
    from PIL import Image

    base = os.path.join(assets, "shared", "images", "resultScreen")
    problemas = []
    difs = {}
    for nome in DIFICULDADES:
        src = os.path.join(base, "diff_%s.png" % nome)
        if not os.path.isfile(src):
            problemas.append("topbar: falta %s" % src)
            continue
        im = Image.open(src).convert("RGBA")
        w = max(1, int(round(im.width * HALF)))
        h = max(1, int(round(im.height * HALF)))
        if w > MAX_TEX or h > MAX_TEX:
            problemas.append("topbar: diff_%s fica %dx%d (limite %d)"
                             % (nome, w, h, MAX_TEX))
            continue
        im = im.resize((w, h), Image.LANCZOS)
        arquivo = "diff_%s.rgba" % nome
        with open(os.path.join(out, arquivo), "wb") as f:
            f.write(im.tobytes())
        difs[nome] = {"arquivo": "resultado/" + arquivo, "w": w, "h": h,
                      "vram_bytes": w * h * 4}
    if problemas:
        return None, problemas

    barra = geometria_da_barra()
    dado = {
        "barra": barra,
        "dificuldades": difs,
        "dificuldade": {
            # `difficulty = new FlxSprite(555 + gameNotchSize.x)` -- so' o x.
            "x": round(555.0 * HALF, 3),
            # `diffYTween = 122` mais a correcao da barra.
            "y_alvo": round(122.0 * HALF + barra["correcao_y"], 3),
            "atraso_ms": 800,   # startDelay: 0.8
            "tween_ms": 500,    # FlxTween.tween(difficulty, {y: ...}, 0.5)
            "linha": "ResultState.hx L332-334 e L736-758",
        },
        "linha": "BitmapUtil.hx L11-25 (barra) e ResultState.hx L342-360",
    }
    return dado, problemas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--out",
                    default=os.path.join(PROJECT, "build", "packs", "resultado"))
    args = ap.parse_args()
    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets>")
        return 1
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        print("  FALHA: falta o Pillow (PYTHONPATH=/tmp/pylibs)")
        return 1
    os.makedirs(args.out, exist_ok=True)
    dado, problemas = empacotar(args.assets, args.out)
    for p in problemas:
        print("  " + p)
    if not dado:
        return 1
    plano = json.load(open(PLANO, encoding="utf-8"))
    plano["topbar"] = dado
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    b = dado["barra"]
    print("barra preta: %dx%d na fonte, borda de %.1f a %.1f px (640x360), "
          "0 B de VRAM" % (b["largura_src"], b["altura_src"], b["borda_y0"],
                           b["borda_y1"]))
    for nome, d in dado["dificuldades"].items():
        print("  diff_%-9s %3dx%-2d  %6d B" % (nome, d["w"], d["h"],
                                               d["vram_bytes"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
