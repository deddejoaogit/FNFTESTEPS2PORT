#!/usr/bin/env python3
"""Empacota o SELO DE RECORDE da tela de Resultado (`highscoreNew`).

O QUE A FONTE FAZ (`ResultState.hx` v0.8.6)
------------------------------------------
    highscoreNew = new FlxSprite(44 + FullScreenScaleMode.gameNotchSize.x, 557);
    // L408-414
    highscoreNew.frames = Paths.getSparrowAtlas('resultScreen/highscoreNew');
    highscoreNew.animation.addByPrefix("new", "highscoreAnim0", 24, false);
    highscoreNew.visible = false;
    highscoreNew.zIndex = 1200;
    add(highscoreNew);
    // L416-427
    new FlxTimer().start(rank.getHighscoreDelay(), _ -> {
      if (params.isNewHighscore ?? false) {
        highscoreNew.visible = true;
        highscoreNew.animation.play("new");
        highscoreNew.animation.onFinish.add(_ -> highscoreNew.animation.play("new", true, false, 16));
      } else highscoreNew.visible = false;
    });

Ou seja: 29 quadros a 24 fps nao loopados; ao terminar, toca de novo A PARTIR DO
QUADRO 16 (`play("new", true, false, 16)`) e, como o `looped` do anim e' false,
isso se repete -- na pratica, 0..28 uma vez e depois 16..28 para sempre.
Posicao (44, 557) em 1280x720 com `gameNotchSize` 0 (o PS2 nao tem entalhe),
`zIndex` 1200 (o ultimo grupo da tela, junto das folhas).

POR QUE ELE NAO ESTAVA NA TELA
------------------------------
Nao era arte faltando: era o DADO. `params.isNewHighscore` vem de
`Save.instance.isSongHighScore(...)` e o port nao tinha save nenhum. Agora tem
(`src/engine/save.c`), e o selo pode acender.

A CONTA DA VRAM (medida, nao estimada)
--------------------------------------
O canvas do Sparrow e' 844x99 (o `frameWidth/frameHeight` do XML), nao os
1024x512 do spritesheet: os quadros sao recortes de uma faixa. Na metade da
resolucao e a 16 bpp isso da' **42.200 B** -- cabe nos 217.856 B que o pior rank
(o SHIT) deixa livres depois da animacao do personagem. Nao ha' degradacao a
fazer aqui, e o empacotador ABORTA se um dia o canvas nao couber no `--limite`.

FORMATO
-------
CFZ4 (um bloco LZ4 por quadro, lido por `engine/pack_stream.c`), como a maquina
do `soundSystem` e as folhas: na RAM fica a tabela, na VRAM um quadro.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_highscore.py --assets /tmp/fassets
    tools/empacotar_highscore.py --ver
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")

# Tudo da fonte (as linhas estao no topo deste arquivo).
ARQUIVO = "resultScreen/highscoreNew"
PREFIXO = "highscoreAnim0"
X = 44.0
Y = 557.0
FPS = 24
HALF = 0.5
QUADRO_LOOP = 16          # `play("new", true, false, 16)`
BPP = 16

# O orcamento: o que a tela tem livre no PIOR rank, depois da animacao do
# personagem do rank (medido no harness: `resultado_rrf_vram` do rank 0 deixa
# 217.856 B).
LIMITE_PADRAO = 217856


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
    """Os nomes da animacao, em ordem -- como o `addByPrefix` do flixel."""
    achados = []
    for nome in quadros:
        if not nome.startswith(prefixo):
            continue
        resto = nome[len(prefixo):]
        if resto == "":
            achados.append((-1, nome))
            continue
        if not resto.isdigit():
            continue
        achados.append((int(resto), nome))
    achados.sort()
    return [n for _i, n in achados]


def canvas_da_animacao(quadros, ids):
    """(w, h) do canvas: o `frameWidth/frameHeight` do XML quando existe."""
    fw = fh = 0
    for k in ids:
        q = quadros[k]
        if q["fw"]:
            return q["fw"], q["fh"]
        fw = max(fw, -q["fx"] + q["w"])
        fh = max(fh, -q["fy"] + q["h"])
    return fw, fh


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
    w = max(1, int(round(cw_src * HALF)))
    h = max(1, int(round(ch_src * HALF)))
    bytes_vram = w * h * (BPP // 8)
    if bytes_vram > limite:
        return {"erro": "o canvas %dx%d a %d bpp custa %d B e o limite e' %d"
                        % (w, h, BPP, bytes_vram, limite)}

    folha = Image.open(png).convert("RGBA")
    blocos = []
    for k in ids:
        q = quadros[k]
        pedaco = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
        tela = Image.new("RGBA", (cw_src, ch_src), (0, 0, 0, 0))
        tela.paste(pedaco, (-q["fx"], -q["fy"]))
        if (w, h) != (cw_src, ch_src):
            tela = tela.resize((w, h), pp.Image.LANCZOS)
        blocos.append(lz4.block.compress(pp.to_ps2_bytes(tela, BPP),
                                         store_size=False))
    caminho = os.path.join(out, "highscoreNew.cfz4")
    partes = pp.write_packs(blocos, w * h * (BPP // 8), caminho)

    return {
        "id": "highscoreNew",
        "prefixo": PREFIXO,
        "fonte": ARQUIVO,
        "src_w": cw_src, "src_h": ch_src,
        "escala": HALF,
        # O desenho multiplica por `fator`: o pacote esta' em `escala` do
        # original e a tela e' a metade, entao w*fator poe o selo no tamanho da
        # fonte.
        "fator": round(HALF / HALF, 4),
        "w": w, "h": h, "bpp": BPP,
        "quadros": len(ids),
        "fps": FPS, "looped": False, "quadro_loop": QUADRO_LOOP,
        "x": round(X * HALF, 3), "y": round(Y * HALF, 3),
        "tex": "resultado/highscoreNew.cfz4",
        "partes": [os.path.basename(p[0]) for p in partes],
        "vram_bytes": bytes_vram,
        "limite": limite,
        "linha": "ResultState.hx L149 e L408-427 (posicao, 24 fps, quadro 16 "
                 "do loop, zIndex 1200, rank.getHighscoreDelay())",
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs",
                                                  "resultado"))
    ap.add_argument("--limite", type=int, default=LIMITE_PADRAO)
    ap.add_argument("--ver", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.ver:
        if not os.path.isfile(PLANO):
            print("  FALHA: nao ha' plano em %s" % PLANO)
            return 1
        d = json.load(open(PLANO, encoding="utf-8")).get("highscore")
        if not d:
            print("  FALHA: o plano nao tem a secao `highscore`")
            return 1
        print("  %s: %dx%d (%dx%d do original a %.3f), %d quadros a %d fps, "
              "%d B de VRAM, em (%.1f, %.1f), loop a partir do quadro %d"
              % (d["tex"], d["w"], d["h"], d["src_w"], d["src_h"], d["escala"],
                 d["quadros"], d["fps"], d["vram_bytes"], d["x"], d["y"],
                 d["quadro_loop"]))
        return 0

    if args.check:
        if not os.path.isfile(PLANO):
            print("  FALHA: nao ha' plano")
            return 1
        d = json.load(open(PLANO, encoding="utf-8")).get("highscore")
        if not d:
            print("  FALHA: o plano nao tem a secao `highscore`")
            return 1
        caminho = os.path.join(PROJECT, "build", "packs", "resultado",
                               os.path.basename(d["tex"]))
        if not os.path.isfile(caminho):
            print("  FALHA: falta %s" % caminho)
            return 1
        with open(caminho, "rb") as f:
            cab = f.read(16)
        if cab[:4] != b"CFZ4":
            print("  FALHA: %s nao tem magic CFZ4" % caminho)
            return 1
        import struct
        _v, nq, fb = struct.unpack("<III", cab[4:16])
        ok = (nq == d["quadros"] and fb == d["w"] * d["h"] * (d["bpp"] // 8))
        print("  %s: %d quadros, %d B/quadro, %d B em disco -- %s"
              % (os.path.basename(caminho), nq, fb, os.path.getsize(caminho),
                 "OK" if ok else "ERRADO"))
        return 0 if ok else 1

    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets>")
        return 1
    os.makedirs(args.out, exist_ok=True)
    d = empacotar(args.assets, args.out, args.limite)
    if d.get("erro"):
        print("  FALHA: " + d["erro"])
        return 1
    plano = {}
    if os.path.isfile(PLANO):
        plano = json.load(open(PLANO, encoding="utf-8"))
    plano["highscore"] = d
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("highscoreNew: %s  %dx%d (%dx%d do original), %d quadros, "
          "%d B de VRAM" % (d["tex"], d["w"], d["h"], d["src_w"], d["src_h"],
                            d["quadros"], d["vram_bytes"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
