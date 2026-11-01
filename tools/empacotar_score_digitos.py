#!/usr/bin/env python3
"""Empacota o PLACAR da tela de Resultado: os 52 quadros digito do original.

O QUE A FONTE FAZ (`ResultScore.hx`, v0.8.6)
--------------------------------------------
`ResultState.create()` L151 monta

    score = new ResultScore(35 + gameNotchSize.x, 305, 10, score);

ou seja DEZ digitos em x = 35 + 65*i (1280x720), y = 305. Cada digito e' um
`ScoreNum`, que carrega `resultScreen/score-digital-numbers` e registra 12
animacoes: ZERO..NINE (5 quadros cada, 24 fps, `looped = false`) mais DISABLED
e GONE (1 quadro cada). `10` e' o valor "sem digito" -> DISABLED.

O que o numero faz no tempo (L17-75 e L108-180 do mesmo arquivo):

  37/24 s   `score.visible = true; score.animateNumbers();`
            Para cada digito i de (10 - digitos) ate 9, um FlxTimer de
            (i-1)/24 s chama `playAnim()` (DISABLED no quadro 0) e `shuffle()`.
  shuffle   um FlxTimer de 1/24 s, 41 voltas: o digito incrementa (10->0->1...)
            e cada troca toca a animacao do novo digito. A PRIMEIRA troca toca
            no quadro 0 (`glow` true, depois vira false) e as seguintes no
            quadro 4 -- o ultimo, que o `looped = false` segura na tela.
  tween     `FlxTween.num(0, finalDigit, 23/24, quadOut)` -- o digito cai no
            valor final seguindo 2t - t^2.
  flicker   depois do tween, `FlxTimer` de `finalDelay/24` s e a animacao do
            digito final recomeca no quadro 0: um ultimo piscar de 5 quadros.

POR QUE UM ATLAS E NAO CFZ4
---------------------------
Na tela os dez digitos aparecem AO MESMO TEMPO, cada um num digito diferente,
e durante o shuffle qualquer um dos dez valores pode estar em qualquer
posicao. Nao existe "um quadro por vez" como nas folhas animadas: o conjunto
inteiro precisa estar residente. Por isso os 52 quadros vao num unico RGBA
(compactados por prateleira, na metade da resolucao), e o jogo desenha
sub-retangulos dele. O custo em VRAM sai no plano (`score.vram_bytes`).

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_score_digitos.py --assets /tmp/fassets
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")

# `ScoreNum.numToString` (ResultScore.hx L132-144), na ordem do indice.
NOMES = ["ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN",
         "EIGHT", "NINE"]

QUADROS_POR_DIGITO = 5      # as 5 subtexturas "X DIGITAL0001..0005"
DIGITO_DISABLED = 10        # indice de DISABLED em numToString
DIGITO_GONE = 11            # o "GONE" que `set_finalDigit` toca

HALF = 0.5
MAX_TEX = 1024          # o limite do GS, repetido aqui so' para a mensagem


# O empacotamento de quadros num atlas (ler o XML, cortar, prateleiras) e' o
# mesmo em todas as pecas desta tela: mora em tools/atlas_frames.py.
sys.path.insert(0, HERE)
import atlas_frames as af  # noqa: E402

ler_xml = af.ler_xml
quadros_do_prefixo = af.quadros_do_prefixo


def empacotar(assets, out):
    """Devolve (dict do plano, lista de problemas)."""
    from PIL import Image

    base = os.path.join(assets, "shared", "images", "resultScreen")
    png = os.path.join(base, "score-digital-numbers.png")
    xml = os.path.join(base, "score-digital-numbers.xml")
    problemas = []
    if not (os.path.isfile(png) and os.path.isfile(xml)):
        return None, ["score: falta %s (ou o .xml)" % png]

    quadros = ler_xml(xml)
    folha = Image.open(png).convert("RGBA")

    # (indice do digito, indice do quadro, nome no XML)
    entradas = []
    for d, nome in enumerate(NOMES):
        ids = quadros_do_prefixo(quadros, nome + " DIGITAL")
        if len(ids) != QUADROS_POR_DIGITO:
            problemas.append("score: %s tem %d quadros (esperado %d)"
                             % (nome, len(ids), QUADROS_POR_DIGITO))
            return None, problemas
        for k, n in enumerate(ids):
            entradas.append((d, k, n))
    for d, prefixo in ((DIGITO_DISABLED, "DISABLED"), (DIGITO_GONE, "GONE")):
        ids = quadros_do_prefixo(quadros, prefixo)
        if len(ids) != 1:
            problemas.append("score: %s tem %d quadros (esperado 1)"
                             % (prefixo, len(ids)))
            return None, problemas
        entradas.append((d, 0, ids[0]))

    # Corta cada quadro e reduz para a metade ANTES de empacotar: assim o atlas
    # nao carrega pixel que a tela de 640x360 nao mostra.
    pecas = []
    for d, k, n in entradas:
        q = quadros[n]
        cru = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
        aw = max(1, int(round(cru.width * HALF)))
        ah = max(1, int(round(cru.height * HALF)))
        if aw > MAX_TEX or ah > MAX_TEX:
            problemas.append("score: %s fica %dx%d (limite %d)"
                             % (n, aw, ah, MAX_TEX))
            return None, problemas
        pecas.append({"digito": d, "quadro": k, "nome": n,
                      "img": cru.resize((aw, ah), Image.LANCZOS),
                      "aw": aw, "ah": ah,
                      "ox": int(round(-q["fx"] * HALF)),
                      "oy": int(round(-q["fy"] * HALF)),
                      "sw": q["w"], "sh": q["h"],
                      "cw": int(round(q["fw"] * HALF)),
                      "ch": int(round(q["fh"] * HALF))})

    escolha = af.escolher_largura([(p["aw"], p["ah"]) for p in pecas])
    if not escolha:
        problemas.append("score: os quadros nao cabem num atlas de %d px"
                         % af.MAX_TEX)
        return None, problemas
    largura, atlas_h, pos = escolha
    for p, (ax, ay) in zip(pecas, pos):
        p["ax"], p["ay"] = ax, ay

    atlas = Image.new("RGBA", (largura, atlas_h), (0, 0, 0, 0))
    for p in pecas:
        atlas.paste(p["img"], (p["ax"], p["ay"]))

    nome_atlas = "score_digitos.rgba"
    with open(os.path.join(out, nome_atlas), "wb") as f:
        f.write(atlas.tobytes())

    # Ordem ESTAVEL na tabela: digito 0..9 x quadro 0..4, depois DISABLED e GONE.
    # E' a mesma que a funcao `res_score_indice` do C usa.
    pecas.sort(key=lambda p: (p["digito"], p["quadro"]))
    frames = [{"digito": p["digito"], "quadro": p["quadro"],
               "ax": p["ax"], "ay": p["ay"], "aw": p["aw"], "ah": p["ah"],
               "ox": p["ox"], "oy": p["oy"],
               "cw": p["cw"], "ch": p["ch"],
               "src_w": p["sw"], "src_h": p["sh"]} for p in pecas]

    dado = {
        "atlas": "resultado/" + nome_atlas,
        "w": largura, "h": atlas_h,
        "vram_bytes": largura * atlas_h * 4,
        "quadros": len(frames),
        "quadros_por_digito": QUADROS_POR_DIGITO,
        "digito_disabled": DIGITO_DISABLED,
        "digito_gone": DIGITO_GONE,
        "frames": frames,
        # ResultState.hx L151: new ResultScore(35, 305, 10, score) em 1280x720.
        "x": round(35.0 * HALF, 3), "y": round(305.0 * HALF, 3),
        "dx": round(65.0 * HALF, 3), "slots": 10,
        # new FlxTimer().start(37/24) -> score.visible = true e animateNumbers()
        "inicio_q": 37,
        # shuffle(): start(1/24, ..., Std.int((41/24)/(1/24))) = 41 voltas
        "shuffle_q": 41,
        # finishShuffleTween(): FlxTween.num(0, finalDigit, 23/24, quadOut)
        "tween_q": 23,
        # animation.addByPrefix(..., 24, false): 24 fps
        "fps": 24,
        "linha": "ResultScore.hx L17-75/L108-180 e ResultState.hx L151",
    }
    return dado, problemas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "resultado"))
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
    plano["score"] = dado
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("placar: %d quadros num atlas %dx%d, %d B de VRAM"
          % (dado["quadros"], dado["w"], dado["h"], dado["vram_bytes"]))
    print("  atlas: %s" % dado["atlas"])
    print("  origem: %s" % dado["linha"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
