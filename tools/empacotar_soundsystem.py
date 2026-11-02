#!/usr/bin/env python3
"""Empacota a MAQUINA DA TELA DE RESULTADO (`soundSystem`).

O QUE A FONTE FAZ (`ResultState.hx` v0.8.6)
------------------------------------------
    var soundSystem = FunkinSprite.createSparrow(-15, -180, 'resultScreen/soundSystem');
    soundSystem.animation.addByPrefix("idle", "sound system", 24, false);
    soundSystem.visible = false;
    new FlxTimer().start(8 / 24, _ -> {
      soundSystem.animation.play("idle");
      soundSystem.visible = true;
    });
    soundSystem.zIndex = 1100;

Ou seja: e' a maquina que cai atras do placar, **em toda partida** (o
comentario do original diz "Plays every time!"), 65 quadros a 24 fps, NAO
loopada -- o `looped = false` do flixel congela no ultimo quadro e ele fica na
tela ate' o fim. Posicao `(-15, -180)` em 1280x720, desenhada com
`gameNotchSize` (0 sem entalhe) e `zIndex` 1100: acima da barra do topo (1010),
abaixo das folhas (1200) e do placar.

POR QUE ELA NAO ESTAVA NA TELA
------------------------------
O canvas e' 875x963 e cada quadro e' um recorte grande dele: a meia resolucao
custava 841 KB por quadro, e a tela inteira tem 1,2 MB. Esta build abre espaco
em duas fontes -- as tres folhas a 0,4 do original (desenhadas a 1,25x, so'
nitidez) e o fundo, que deixou de ser textura e virou oito faixas -- e gasta o
que sobrou NESTA peca, que e' a unica que ainda faltava e que toca sempre.

A escala nao e' escolhida a mao: o empacotador desce de 0,5 em passos de 0,005
ate' o canvas caber no orcamento (`--limite`, medido no harness por
`resultado_vram`). O desenho multiplica por `fator = 0,5 / escala`, entao a
maquina sai no MESMO lugar e no MESMO tamanho da fonte -- o que muda e' so' a
nitidez.

FORMATO
-------
CFZ4 (o mesmo das folhas e dos personagens), um bloco LZ4 por quadro, lido no
console por `engine/pack_stream.c`: o jogo le' UM quadro por vez do pendrive e
a VRAM paga um quadro.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_soundsystem.py --assets /tmp/fassets
    tools/empacotar_soundsystem.py --ver | --check
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")

# Tudo da fonte (as linhas estao no topo deste arquivo).
ARQUIVO = "resultScreen/soundSystem"
PREFIXO = "sound system"
X = -15.0
Y = -180.0
INICIO_Q = 8            # new FlxTimer().start(8 / 24, ...)
FPS = 24
HALF = 0.5

# O orcamento padrao: o que a tela tem livre DEPOIS das tres folhas a 0,4 e do
# fundo em faixas. Medido no harness (`resultado_vram`), nao estimado -- se
# mudar outra peca da tela, este numero tem de ser remedido.
LIMITE_PADRAO = 190000


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


def quadros_da_animacao(quadros, prefixo):
    """Os nomes da animacao, em ordem -- como o `addByPrefix` do flixel.

    O prefixo aqui nao termina em digito ("sound system"), entao o numero vem
    logo depois dele: "sound system0000" .. "sound system0064".
    """
    achados = []
    for nome in quadros:
        if not nome.startswith(prefixo):
            continue
        resto = nome[len(prefixo):]
        if resto == "":
            # `addByPrefix` tambem aceita o nome EXATO (o "0000" do flixel e' o
            # primeiro quadro quando o prefixo nao tem numero).
            achados.append((-1, nome))
            continue
        if not resto.isdigit():
            continue
        achados.append((int(resto), nome))
    achados.sort()
    return [n for _i, n in achados]


def canvas_da_animacao(quadros, ids):
    """(w, h) do canvas: o maior `-frameX + width` e `-frameY + height`.

    E' o `frameWidth/frameHeight` do XML quando existe; quando nao existe, o
    canvas e' o que couber todos os quadros, que e' o que o desenho do port
    precisa (ele desenha o canvas inteiro no lugar do sprite).
    """
    fw = fh = 0
    for k in ids:
        q = quadros[k]
        if q["fw"]:
            return q["fw"], q["fh"]
        fw = max(fw, -q["fx"] + q["w"])
        fh = max(fh, -q["fy"] + q["h"])
    return fw, fh


def escala_que_cabe(w, h, limite):
    """A maior escala do original, de 0,5 para baixo, cujo canvas cabe."""
    s = HALF
    while s >= 0.05:
        cw = max(1, int(round(w * s)))
        ch = max(1, int(round(h * s)))
        if cw <= 1024 and ch <= 1024 and cw * ch * 4 <= limite:
            return s, cw, ch
        s = round(s - 0.005, 4)
    return None, 0, 0


def empacotar(assets, out, limite):
    try:
        from PIL import Image
    except ImportError:
        return {"erro": "sem Pillow (use PYTHONPATH=/tmp/pylibs)"}
    import lz4.block
    sys.path.insert(0, HERE)
    import pacote_personagem as pp

    base = os.path.join(assets, "shared", "images")
    png = os.path.join(base, ARQUIVO + ".png")
    xml = os.path.join(base, ARQUIVO + ".xml")
    for c in (png, xml):
        if not os.path.isfile(c):
            return {"erro": "falta %s" % c}

    quadros = ler_xml(xml)
    ids = quadros_da_animacao(quadros, PREFIXO)
    if not ids:
        return {"erro": "nenhum quadro com o prefixo %r" % PREFIXO}
    cw_src, ch_src = canvas_da_animacao(quadros, ids)
    esc, w, h = escala_que_cabe(cw_src, ch_src, limite)
    if not esc:
        return {"erro": "nao ha escala que caiba em %d B" % limite}

    folha = Image.open(png).convert("RGBA")
    blocos = []
    for k in ids:
        q = quadros[k]
        pedaco = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
        tela = Image.new("RGBA", (cw_src, ch_src), (0, 0, 0, 0))
        tela.paste(pedaco, (-q["fx"], -q["fy"]))
        if (w, h) != (cw_src, ch_src):
            tela = tela.resize((w, h), pp.Image.LANCZOS)
        blocos.append(lz4.block.compress(pp.to_ps2_bytes(tela, 32),
                                         store_size=False))
    caminho = os.path.join(out, "soundsystem.cfz4")
    partes = pp.write_packs(blocos, w * h * 4, caminho)

    dado = {
        "id": "soundsystem",
        "prefixo": PREFIXO,
        "fonte": ARQUIVO,
        "src_w": cw_src, "src_h": ch_src,
        "escala": esc,
        # O desenho multiplica por `fator`: o pacote esta' em `esc` do original
        # e a tela e' a metade, entao w*fator poe a maquina no tamanho da fonte.
        "fator": round(HALF / esc, 4),
        "w": w, "h": h,
        "quadros": len(ids),
        "fps": FPS, "looped": False,
        "x": round(X * HALF, 3), "y": round(Y * HALF, 3),
        "inicio_ms": round(1000.0 * INICIO_Q / FPS),
        "tex": "resultado/soundsystem.cfz4",
        "partes": [os.path.basename(p[0]) for p in partes],
        "vram_bytes": w * h * 4,
        "limite": limite,
        "linha": "ResultState.hx L189-199 (posicao, 8/24 s, 24 fps, zIndex 1100)",
    }
    return {"dado": dado}


def juntar(dado):
    """Le o plano, poe (ou troca) o bloco da maquina e grava."""
    plano = {}
    if os.path.isfile(PLANO):
        with open(PLANO, encoding="utf-8") as fp:
            plano = json.load(fp)
    plano["soundsystem"] = dado
    with open(PLANO, "w", encoding="utf-8") as fp:
        json.dump(plano, fp, indent=1, ensure_ascii=False)
        fp.write("\n")
    return plano


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--out")
    ap.add_argument("--limite", type=int, default=LIMITE_PADRAO)
    ap.add_argument("--ver", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.ver:
        plano = json.load(open(PLANO, encoding="utf-8"))
        d = plano.get("soundsystem")
        if not d:
            print("plano sem o bloco `soundsystem`")
            return 1
        print("soundSystem: %d quadros, canvas %dx%d (escala %.3f do original)"
              % (d["quadros"], d["w"], d["h"], d["escala"]))
        print("  desenho %.1fx%.1f em (%.1f, %.1f), entra em %d ms, %d B de VRAM"
              % (d["w"] * d["fator"], d["h"] * d["fator"], d["x"], d["y"],
                 d["inicio_ms"], d["vram_bytes"]))
        return 0
    if args.check:
        plano = json.load(open(PLANO, encoding="utf-8"))
        d = plano.get("soundsystem")
        if not d:
            print("FALHA: plano sem o bloco `soundsystem`")
            return 1
        print("ok: soundsystem %dx%d, %d B de VRAM"
              % (d["w"], d["h"], d["vram_bytes"]))
        return 0

    if not args.assets:
        ap.error("--assets e' obrigatorio (a raiz do funkin.assets)")
    out = args.out or os.path.join(PROJECT, "build", "packs", "resultado")
    os.makedirs(out, exist_ok=True)
    r = empacotar(args.assets, out, args.limite)
    if "erro" in r:
        print("  " + r["erro"])
        return 1
    d = r["dado"]
    juntar(d)
    print("soundSystem: %d quadros, canvas %dx%d (escala %.3f do original)"
          % (d["quadros"], d["w"], d["h"], d["escala"]))
    print("  desenho %.1fx%.1f em (%.1f, %.1f), entra em %d ms"
          % (d["w"] * d["fator"], d["h"] * d["fator"], d["x"], d["y"],
             d["inicio_ms"]))
    print("  %d B de VRAM (limite %d), pacote %s"
          % (d["vram_bytes"], d["limite"], ", ".join(d["partes"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
