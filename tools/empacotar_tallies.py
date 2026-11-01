#!/usr/bin/env python3
"""Empacota os CONTADORES DE JULGAMENTO da tela de Resultado (`TallyCounter`).

O QUE A FONTE FAZ (`TallyCounter.hx`, v0.8.6)
---------------------------------------------
`ResultState.create()` L435-476 cria SETE contadores, cada um na sua posicao e
com a sua cor:

    totalNotesHit  (375, 150)  0xFFFFFF     (x -= 30 se totalNotesHit >= 1000)
    maxCombo       (375, 200)  0xFFFFFF     (idem)
    sick           (230, 277)  0xFF89E59E
    good           (210, 331)  0xFF89C9E5
    bad            (190, 385)  0xFFE6CF8A
    shit           (220, 439)  0xFFE68C8A
    missed         (260, 493)  0xFFC68AE6

Cada `TallyCounter` e' um grupo de `TallyNumber`, que carrega o atlas
`resultScreen/tallieNumber` (dez animacoes; `i + " small"`, um quadro cada) e
desenha um sprite por digito, com **43 px** de espacamento, na cor do contador.
O numero cresce ate' o alvo com um `FlxTween` de 0,5 s (`FlxEase.quartOut`) que
recomeca a cada quadro enquanto `curNumber < neededNumber` (L36-40 e L42-77).

O contador i aparece em `(0.3 * i) + 1.20` s (ResultState L481-488).

ESCALA
------
A tela do original e' 1280x720 e o port desenha 640x360: tudo pela metade.
O atlas aqui e' pequeno (dez glifos de ~35x43), e por isso ele e' empacotado
por recorte -- sem canvas vazio.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_tallies.py --assets /tmp/fassets
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

HALF = 0.5

# Os sete contadores, na ordem do `ratingGrp` (ResultState.hx L435-476).
# `campo` e' qual numero do jogo cada um mostra; a ordem importa porque o
# atraso de entrada e' `(0.3 * ind) + 1.20` s com `ind` = posicao no grupo.
CONTADORES = [
    # (campo, x, y, cor RGB)
    ("total_hit", 375.0, 150.0, (0xFF, 0xFF, 0xFF)),
    ("max_combo", 375.0, 200.0, (0xFF, 0xFF, 0xFF)),
    ("sick", 230.0, 277.0, (0x89, 0xE5, 0x9E)),
    ("good", 210.0, 331.0, (0x89, 0xC9, 0xE5)),
    ("bad", 190.0, 385.0, (0xE6, 0xCF, 0x8A)),
    ("shit", 220.0, 439.0, (0xE6, 0x8C, 0x8A)),
    ("missed", 260.0, 493.0, (0xC6, 0x8A, 0xE6)),
]


def empacotar(assets, out):
    """Devolve (dict do plano, lista de problemas)."""
    from PIL import Image

    base = os.path.join(assets, "shared", "images", "resultScreen")
    png = os.path.join(base, "tallieNumber.png")
    xml = os.path.join(base, "tallieNumber.xml")
    problemas = []
    if not (os.path.isfile(png) and os.path.isfile(xml)):
        return None, ["tallies: falta %s (ou o .xml)" % png]

    quadros = af.ler_xml(xml)
    folha = Image.open(png).convert("RGBA")

    pecas = []
    for d in range(10):
        # `for (i in 0...10) animation.addByPrefix(Std.string(i), i + " small")`
        ids = af.quadros_do_prefixo(quadros, "%d small" % d)
        if len(ids) != 1:
            problemas.append("tallies: o digito %d tem %d quadros (esperado 1)"
                             % (d, len(ids)))
            return None, problemas
        q = quadros[ids[0]]
        cru = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
        aw = max(1, int(round(cru.width * HALF)))
        ah = max(1, int(round(cru.height * HALF)))
        pecas.append({"digito": d,
                      "img": cru.resize((aw, ah), Image.LANCZOS),
                      "aw": aw, "ah": ah,
                      "ox": int(round(-q["fx"] * HALF)),
                      "oy": int(round(-q["fy"] * HALF)),
                      "src_w": q["w"], "src_h": q["h"]})

    escolha = af.escolher_largura([(p["aw"], p["ah"]) for p in pecas])
    if not escolha:
        problemas.append("tallies: os glifos nao cabem num atlas de %d px"
                         % af.MAX_TEX)
        return None, problemas
    largura, altura, pos = escolha
    for p, (ax, ay) in zip(pecas, pos):
        p["ax"], p["ay"] = ax, ay

    atlas = Image.new("RGBA", (largura, altura), (0, 0, 0, 0))
    for p in pecas:
        atlas.paste(p["img"], (p["ax"], p["ay"]))
    nome = "tallies.rgba"
    with open(os.path.join(out, nome), "wb") as f:
        f.write(atlas.tobytes())

    frames = [{"digito": p["digito"],
               "ax": p["ax"], "ay": p["ay"], "aw": p["aw"], "ah": p["ah"],
               "ox": p["ox"], "oy": p["oy"],
               "src_w": p["src_w"], "src_h": p["src_h"]} for p in pecas]

    dado = {
        "atlas": "resultado/" + nome,
        "w": largura, "h": altura,
        "vram_bytes": largura * altura * 4,
        "frames": frames,
        # `TallyNumber`: um sprite por digito, 43 px de espacamento (1280x720).
        "dx": round(43.0 * HALF, 3),
        # `new FlxTimer().start((0.3 * ind) + 1.20, ...)` e o tween de 0,5 s.
        "atraso_base_ms": 1200,
        "atraso_passo_ms": 300,
        "tween_ms": 500,
        # L435-447: totalNotesHit e maxCombo andam 30 px para a esquerda quando
        # o total de notas passadas de 1000, para o numero de quatro digitos
        # caber na tela.
        "ajuste_1000": 30,
        "contadores": [
            {"campo": campo,
             "x": round(x * HALF, 3), "y": round(y * HALF, 3),
             "r": cor[0], "g": cor[1], "b": cor[2]}
            for campo, x, y, cor in CONTADORES
        ],
        "linha": "TallyCounter.hx L14-90 e ResultState.hx L418-490",
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
    plano["tallies"] = dado
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("tallies: %d glifos num atlas %dx%d, %d B de VRAM"
          % (len(dado["frames"]), dado["w"], dado["h"], dado["vram_bytes"]))
    print("  atlas: %s" % dado["atlas"])
    print("  origem: %s" % dado["linha"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
