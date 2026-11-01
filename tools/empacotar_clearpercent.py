#!/usr/bin/env python3
"""Empacota o PERCENTUAL DE CONCLUSAO da tela de Resultado (`ClearPercentCounter`).

O QUE A FONTE FAZ (`ClearPercentCounter.hx`, v0.8.6)
---------------------------------------------------
`ResultState.create()` L137 cria um contador pequeno no topo (o `%` da barra) e
`startRankTallySequence()` L585-646 cria o GRANDE, no meio da tela:

    var clearPercentCounter = new ClearPercentCounter(640 + 190, 360 - 70,
                                                      clearPercentLerp);
    FlxTween.tween(clearPercentCounter, {curNumber: clearPercentTarget},
                   58 / 24, {ease: quartOut, ...});

com `clearPercentTarget = floor(tallyCompletion * 100)` e
`clearPercentLerp = max(0, target - 36)` -- o numero comeca 36 abaixo e sobe.

Cada `ClearPercentCounter` e' um grupo: UM sprite de texto
(`clearPercentText`, "CLEAR" com o `%`) em (0,0) e um `ClearPercentNumber` por
digito, com `digitSize = 72` px de espacamento e `yPos = 72` (o texto fica em
cima). A variante do glifo depende da posicao (L92-99):

    len == 1  -> um digito, empurrado 1 para a direita (`digitOffset = 1`)
    len == 3  -> empurrado 1 para a esquerda, e SO' o primeiro usa `Left`
    senao     -> `digitOffset = 0` e TODOS usam `Right`

No fim do tween o contador pisca (`flash(true)`, 0,4 s), e 0,25 s depois o alpha
cai para 0 em 0,5 s com 0,5 s de atraso -- depois ele sai da cena (L620-640).

ESCALA E VRAM
-------------
Os cinco atlas originais, na metade, custariam 190.348 B -- e a tela ja' tem o
placar (508.480 B) e os contadores dentro de 1.261.568 B. Os glifos grandes vao
entao a **0,4** do original (desenhados a 1,25x o tamanho guardado): e' a
degradacao que cabe, e ela nao muda nenhum evento -- so' a nitidez.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_clearpercent.py --assets /tmp/fassets
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")

sys.path.insert(0, HERE)
import atlas_frames as af  # noqa: E402

HALF = 0.5          # a escala da TELA (640x360)
ESCALA_GLIFO = 0.4  # a escala em que os glifos grandes vao para a VRAM

# Os cinco pedacos, com o nome que o `ClearPercentCounter` usa.
ATLASES = [
    # (id, arquivo do funkin.assets, escala de armazenamento)
    ("left", "clearPercent/clearPercentNumberLeft", ESCALA_GLIFO),
    ("right", "clearPercent/clearPercentNumberRight", ESCALA_GLIFO),
    ("small", "clearPercent/clearPercentNumberSmall", ESCALA_GLIFO),
]


def empacotar(assets, out):
    """Devolve (dict do plano, lista de problemas)."""
    from PIL import Image

    base = os.path.join(assets, "shared", "images", "resultScreen")
    problemas = []
    saida = {"atlas": {}, "texto": {}, "geometria": {}}

    for ident, arquivo, escala in ATLASES:
        png = os.path.join(base, arquivo + ".png")
        xml = os.path.join(base, arquivo + ".xml")
        if not (os.path.isfile(png) and os.path.isfile(xml)):
            return None, ["clearPercent/%s: falta %s (ou o .xml)"
                          % (ident, png)]
        quadros = af.ler_xml(xml)
        folha = Image.open(png).convert("RGBA")
        pecas = []
        for d in range(10):
            ids = af.quadros_do_prefixo(quadros, "number %d " % d)
            if len(ids) != 1:
                problemas.append("clearPercent/%s: o digito %d tem %d quadros"
                                 % (ident, d, len(ids)))
                return None, problemas
            q = quadros[ids[0]]
            cru = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
            aw = max(1, int(round(cru.width * escala)))
            ah = max(1, int(round(cru.height * escala)))
            pecas.append({"digito": d,
                          "img": cru.resize((aw, ah), Image.LANCZOS),
                          "aw": aw, "ah": ah,
                          "ox": int(round(-q["fx"] * HALF)),
                          "oy": int(round(-q["fy"] * HALF)),
                          "src_w": q["w"], "src_h": q["h"]})
        escolha = af.escolher_largura([(p["aw"], p["ah"]) for p in pecas])
        if not escolha:
            return None, ["clearPercent/%s: os glifos nao cabem" % ident]
        largura, altura, pos = escolha
        for p, (ax, ay) in zip(pecas, pos):
            p["ax"], p["ay"] = ax, ay
        atlas = Image.new("RGBA", (largura, altura), (0, 0, 0, 0))
        for p in pecas:
            atlas.paste(p["img"], (p["ax"], p["ay"]))
        nome = "clear_%s.rgba" % ident
        with open(os.path.join(out, nome), "wb") as f:
            f.write(atlas.tobytes())
        saida["atlas"][ident] = {
            "arquivo": "resultado/" + nome,
            "w": largura, "h": altura,
            "vram_bytes": largura * altura * 4,
            # o tamanho DESENHADO e' o do original pela metade; como o glifo
            # foi guardado menor, o fator converte um no outro.
            "fator": round(HALF / escala, 4),
            "frames": [{"digito": p["digito"],
                        "ax": p["ax"], "ay": p["ay"],
                        "aw": p["aw"], "ah": p["ah"],
                        "ox": p["ox"], "oy": p["oy"],
                        "src_w": p["src_w"], "src_h": p["src_h"]}
                       for p in pecas],
        }

    # Os dois textos ("CLEAR" e o "%" pequeno): imagens estaticas, um quadro.
    for ident, arquivo in (("texto", "clearPercent/clearPercentText"),
                           ("texto_small", "clearPercent/clearPercentTextSmall")):
        png = os.path.join(base, arquivo + ".png")
        if not os.path.isfile(png):
            return None, ["clearPercent/%s: falta %s" % (ident, png)]
        im = Image.open(png).convert("RGBA")
        w = max(1, int(round(im.width * ESCALA_GLIFO)))
        h = max(1, int(round(im.height * ESCALA_GLIFO)))
        im = im.resize((w, h), Image.LANCZOS)
        nome = "clear_%s.rgba" % ident
        with open(os.path.join(out, nome), "wb") as f:
            f.write(im.tobytes())
        saida["texto"][ident] = {
            "arquivo": "resultado/" + nome,
            "w": w, "h": h, "vram_bytes": w * h * 4,
            "fator": round(HALF / ESCALA_GLIFO, 4),
        }

    # A geometria, em 640x360.
    saida["geometria"] = {
        # new ClearPercentCounter(640 + 190, 360 - 70, ...)
        "x": round((640.0 + 190.0) * HALF, 3),
        "y": round((360.0 - 70.0) * HALF, 3),
        # digitSize = 72 (grande) / 32 (pequeno); yPos += 72 no grande.
        "passo_grande": round(72.0 * HALF, 3),
        "passo_pequeno": round(32.0 * HALF, 3),
        "y_digitos_grande": round(72.0 * HALF, 3),
        # new ClearPercentCounter(640 + 300, 360 - 100, ..., small = true)
        "small_x": round((640.0 + 300.0) * HALF, 3),
        "small_y": round((360.0 - 100.0) * HALF, 3),
        "small_x_pos": round(-24.0 * HALF, 3),
        "small_y_passo": round(-4.0 * HALF, 3),
        # `timerThenSongName()` (L741-745) reposiciona o contador pequeno:
        #   clearPercentSmall.x = (difficulty.x + difficulty.width) + 60
        #   clearPercentSmall.y = -height, tween ate' (122 - 5) + correcao
        # Em 640x360 a parte fixa do x e' (555 + 60) / 2; a largura da imagem
        # da dificuldade entra em tempo de execucao (muda com a dificuldade).
        "small_fixo_x": round((555.0 + 60.0) * HALF, 3),
        "small_y_alvo": round((122.0 - 5.0) * HALF, 3),
        # O texto (`clearPercentTextSmall`) fica em x = 40 no grupo.
        "small_texto_x": round(40.0 * HALF, 3),
        # Os tempos, em 24 avos (o relogio da tela).
        "inicio_q": 37,          # junto com o `score.animateNumbers()`
        "tween_q": 58,           # FlxTween.tween(..., 58 / 24, quartOut)
        "flash_ms": 400,         # new FlxTimer().start(0.4, flash(false))
        "fade_espera_ms": 250,   # new FlxTimer().start(0.25, ...)
        "fade_atraso_ms": 500,   # FlxTween.tween(..., {alpha: 0}, 0.5,
        "fade_ms": 500,          #   {startDelay: 0.5})
        "abaixo": 36,            # clearPercentLerp = max(0, target - 36)
        "linha": "ClearPercentCounter.hx L22-140 e ResultState.hx L577-646",
    }
    return saida, problemas


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
    plano["clear_percent"] = dado
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    total = sum(a["vram_bytes"] for a in dado["atlas"].values())
    total += sum(t["vram_bytes"] for t in dado["texto"].values())
    for ident, a in dado["atlas"].items():
        print("clearPercent/%-6s %3dx%-3d  %6d B  fator %.2f"
              % (ident, a["w"], a["h"], a["vram_bytes"], a["fator"]))
    for ident, t in dado["texto"].items():
        print("clearPercent/%-6s %3dx%-3d  %6d B" % (ident, t["w"], t["h"],
                                                     t["vram_bytes"]))
    print("total: %d B de VRAM" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
