#!/usr/bin/env python3
"""Empacota a arte da tela de FREEPLAY: as capsulas e o titulo de cada musica.

O QUE A FONTE FAZ (`SongMenuItem.hx`, v0.8.6)
--------------------------------------------
    capsule.frames = Paths.getSparrowAtlas('freeplay/freeplayCapsule/capsule/freeplayCapsule');
    capsule.animation.addByPrefix('selected',   'mp3 capsule w backing0', 24);
    capsule.animation.addByPrefix('unselected', 'mp3 capsule w backing NOT SELECTED', 24);
    ...
    public var realScaled:Float = 0.8;
    public function intendedX(index:Float):Float { return 270 + (60 * (FlxMath.fastSin(index))); }
    public function intendedY(index:Float):Float { return (index * ((capsule.height * realScaled) + 10)) + 120; }

O titulo NAO e' bitmap no original: e' um `FlxText` com a fonte `5by7` em
`Std.int(40 * realScaled)` = 32 px, com `GlowFilter(0xFF00ccff, 1, 5, 5, 210,
MEDIUM)` por cima, em `(capsule.width * 0.26, 45)` do espaco da capsula e
`clipWidth` 255 (`CapsuleText.hx`). Aqui ele vai PRE-RENDERIZADO, um bitmap por
musica -- a mesma escolha do nome na tela de Resultado: o port nao compoe texto
com filtro em tempo de execucao, e a fonte do original e' a que se usa.

O QUE VAI PARA O PENDURIVE
--------------------------
1. `freeplay/capsula.cfz4` -- 16 quadros (8 da animacao `selected`, 8 da
   `unselected`) compostos no canvas SEM CORTE do XML (`frameWidth`x`frameHeight`
   = 612x132) e reduzidos a METADE, como todo o resto da tela: 306x66. CFZ4
   porque so' DOIS quadros ficam na VRAM por vez (um selecionado, um nao): o
   jogo le' um bloco por troca de quadro, a 24 fps, e nao guarda o pacote.
2. `freeplay/tit_<id>.rgba` -- o titulo de cada musica, RGBA cru. Sao 27 e so'
   os ~7 visiveis ficam na VRAM; o resto entra por leitura quando precisa.
3. `assets-data/freeplay-plan.json` + `src/engine/freeplay_dados.h` -- o dado que
   o C le': tamanho do quadro, quantos quadros por animacao, o passo vertical, a
   amplitude do x, as tabelas `xFrames`/`xPosLerpLol` do salto de entrada.

POR QUE A VARIANTE `-pico` NAO E' EMPACOTADA
--------------------------------------------
`players/<dono>.json` traz `freeplayStyle`: "bf" no bf, "pico" no pico. Quem
escolhe e' o personagem JOGAVEL da sessao (o do char select). O port nao tem
char select e o personagem de menu e' sempre o bf, entao o estilo carregado e'
o "bf" -- empacotar a capsula do pico seria arte que nada seleciona. O caminho
fica registrado aqui e o dado do plano diz qual arquivo usar.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_freeplay.py --assets /tmp/fassets
    tools/empacotar_freeplay.py --ver | --check
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "freeplay-plan.json")
REGISTRO = os.path.join(PROJECT, "src", "engine", "song_registry_data.inc")
HEADER = os.path.join(PROJECT, "src", "engine", "freeplay_dados.h")

HALF = 0.5                    # 1280x720 -> 640x360, como todas as telas do port
MAX_TEX = 1024

# A capsula: `SongMenuItem.hx` L84-87. Os prefixos sao os do `addByPrefix`
# L85-86. O `0` no fim do primeiro NAO e' decoracao: `addByPrefix` casa o
# prefixo e ordena o resto como numero, entao 'mp3 capsule w backing0' casa
# "mp3 capsule w backing0000".."mp3 capsule w backing0007" e NAO casa a
# variante "NOT SELECTED".
CAPSULA = {
    "fonte": "freeplay/freeplayCapsule/capsule/freeplayCapsule",
    "sel_prefixo": "mp3 capsule w backing0",
    "nao_prefixo": "mp3 capsule w backing NOT SELECTED",
    "fps": 24,
    "real_scaled": 0.8,       # SongMenuItem.realScaled (L619)
    # L119-122: `songName`/`songText` no espaco da capsula.
    "texto": {
        "fonte_ttf": "fonts/5by7.ttf",
        "tamanho": 32.0,      # Std.int(40 * realScaled)
        "x": 0.26,            # capsule.width * 0.26
        "y": 45.0,
        "clip": 255,          # CapsuleText.clipWidth (L22)
        "cor_glow": 0x00CCFF, # CapsuleText.glowColor (L24)
        "glow_blur": 5.0,     # GlowFilter(..., 5, 5, 210, MEDIUM)
        "glow_forca": 210.0,
    },
}

# `SongMenuItem.update()` L670-700: o salto de entrada anda uma casa por 1/24 s
# e escala a capsula por `xFrames` (x) e o inverso (y).
XFRAMES = [1.7, 1.8, 0.85, 0.85, 0.97, 0.97, 1.0]
XPOS_LERP = [0.0, 0.0, 0.16, 0.16, 0.22, 0.22, 0.245]

# `MathUtil.smoothLerpPrecision(x, alvo, elapsed, t)` suaviza com este tempo.
LERP_X = 0.256                # `SongMenuItem.update()` L735
LERP_Y = 0.192                # L736


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
    """Nomes dos quadros da animacao, em ordem -- como `addByPrefix`."""
    achados = []
    for nome in quadros:
        if not nome.startswith(prefixo):
            continue
        resto = nome[len(prefixo):]
        if not resto or not resto.isdigit():
            continue
        achados.append((int(resto), nome))
    achados.sort()
    return [n for _i, n in achados]


def titulos():
    """[(id, titulo)] do registro de musicas do port, na ordem."""
    texto = open(REGISTRO, encoding="utf-8").read()
    saida = []
    for m in re.finditer(
            r'static const sr_song_t s_[a-z0-9_]+ = \{\s*\n\s*"([a-z0-9\-]+)", "([^"]+)"',
            texto):
        saida.append((m.group(1), m.group(2)))
    return saida


def render_titulo(titulo, ttf, tam, escala):
    """O `FlxText` do original, com o glow, num RGBA do tamanho do texto."""
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
    fonte = ImageFont.truetype(ttf, tam)
    caixa = fonte.getbbox(titulo)
    if not caixa:
        caixa = (0, 0, 1, tam)
    larg = int(caixa[2] - caixa[0])
    alt = int(caixa[3] - caixa[1])
    # Folga para o glow (blur de 5 px em 1280x720 -> escala).
    pad = int(round(CAPSULA["texto"]["glow_blur"] * 0.75))
    w = max(1, int(round((larg + 2 * pad))))
    h = max(1, int(round((alt + 2 * pad))))

    # O texto BRANCO (o `whiteText`) e o halo CIANO (o `blurredText` + glow).
    branco = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(branco)
    d.text((pad - caixa[0], pad - caixa[1]), titulo, font=fonte, fill=255)
    alpha = branco.filter(ImageFilter.GaussianBlur(pad * 0.6))
    forca = CAPSULA["texto"]["glow_forca"] / 255.0
    r, g, b = CAPSULA["texto"]["cor_glow"] >> 16 & 0xFF, \
              CAPSULA["texto"]["cor_glow"] >> 8 & 0xFF, \
              CAPSULA["texto"]["cor_glow"] & 0xFF
    saida = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    saida.paste(Image.new("RGBA", (w, h), (r, g, b, 255)),
                (0, 0), alpha.point(lambda v: int(v * forca)))
    saida.paste(Image.new("RGBA", (w, h), (255, 255, 255, 255)), (0, 0), branco)
    if escala != 1.0:
        saida = saida.resize((max(1, int(round(w * escala))),
                              max(1, int(round(h * escala)))), Image.LANCZOS)
    return saida


def empacotar(assets, out):
    from PIL import Image
    import lz4.block

    sys.path.insert(0, HERE)
    import pacote_personagem as pp

    problemas = []
    base = os.path.join(assets, "preload", "images")
    os.makedirs(out, exist_ok=True)

    # ---- 1. A CAPSULA ---------------------------------------------------
    png = os.path.join(base, CAPSULA["fonte"] + ".png")
    xml = os.path.join(base, CAPSULA["fonte"] + ".xml")
    if not (os.path.isfile(png) and os.path.isfile(xml)):
        return None, None, ["freeplay: falta %s (ou o .xml)" % png]
    quadros = ler_xml(xml)
    sel = quadros_da_folha(quadros, CAPSULA["sel_prefixo"])
    nao = quadros_da_folha(quadros, CAPSULA["nao_prefixo"])
    if not sel or not nao:
        return None, None, ["freeplay: as animacoes da capsula nao apareceram "
                            "(%d selecionados, %d nao)" % (len(sel), len(nao))]
    # O canvas comum: o MAIOR `frameWidth`x`frameHeight` de todos os quadros.
    # E' o que o flixel usa como `capsule.width/height`, e usar o mesmo para as
    # duas animacoes deixa UM retangulo de desenho para as duas.
    cw = max((quadros[k]["fw"] or quadros[k]["w"]) for k in sel + nao)
    ch = max((quadros[k]["fh"] or quadros[k]["h"]) for k in sel + nao)
    w = max(1, int(round(cw * HALF)))
    h = max(1, int(round(ch * HALF)))
    if w > MAX_TEX or h > MAX_TEX:
        return None, None, ["freeplay: capsula %dx%d passa do limite %d"
                            % (w, h, MAX_TEX)]
    folha = Image.open(png).convert("RGBA")
    blocos = []
    for k in sel + nao:
        q = quadros[k]
        pedaco = folha.crop((q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"]))
        tela = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        tela.paste(pedaco, (-q["fx"], -q["fy"]))
        tela = tela.resize((w, h), Image.LANCZOS)
        blocos.append(lz4.block.compress(pp.to_ps2_bytes(tela, 32),
                                         store_size=False))
    partes = pp.write_packs(blocos, w * h * 4,
                            os.path.join(out, "capsula.cfz4"))
    dado_capsula = {
        "fonte": CAPSULA["fonte"],
        "tex": "freeplay/capsula.cfz4",
        "partes": [os.path.basename(p[0]) for p in partes],
        "src_w": cw, "src_h": ch,
        "w": w, "h": h,
        "quadros": len(blocos),
        "sel_ini": 0, "sel_n": len(sel),
        "nao_ini": len(sel), "nao_n": len(nao),
        "fps": CAPSULA["fps"],
        "real_scaled": CAPSULA["real_scaled"],
        "vram_bytes": w * h * 4,
        "linha": "SongMenuItem.hx L84-87 (capsula) e L619 (realScaled)",
    }

    # ---- 2. OS TITULOS --------------------------------------------------
    ttf = os.path.join(assets, CAPSULA["texto"]["fonte_ttf"])
    if not os.path.isfile(ttf):
        return None, None, ["freeplay: falta a fonte %s" % ttf]
    # O tamanho que o original usa, ja' com a escala da tela: 32 px em
    # 1280x720 -> 32 * realScaled(0,8) * metade = 12,8 px aqui.
    tam_src = CAPSULA["texto"]["tamanho"] * CAPSULA["real_scaled"]
    tam = max(6, int(round(tam_src * HALF)))
    escala = (tam_src * HALF) / float(tam)   # o que sobra do arredondamento
    titulos_out = {}
    for sid, titulo in titulos():
        im = render_titulo(titulo, ttf, tam, escala)
        nome = "tit_%s.rgba" % sid
        with open(os.path.join(out, nome), "wb") as f:
            f.write(im.tobytes())
        titulos_out[sid] = {
            "id": sid, "titulo": titulo,
            "arquivo": "freeplay/" + nome,
            "w": im.width, "h": im.height,
            "vram_bytes": im.width * im.height * 4,
        }

    # ---- 3. O DADO -----------------------------------------------------
    # As posicoes saem de `SongMenuItem.intendedX/intendedY` (L762-769) e o
    # `capsule.height` do original e' o canvas SEM CORTE (ch), nao o pedaco.
    cap_h_src = ch * CAPSULA["real_scaled"]
    dado = {
        "capsula": dado_capsula,
        "titulos": titulos_out,
        "escala": HALF,
        # `intendedX(index) = 270 + 60*fastSin(index)`; `intendedY(index) =
        # index*((capsule.height*realScaled) + 10) + 120`.
        "x_base": round(270.0 * HALF, 3),
        "x_amp": round(60.0 * HALF, 3),
        "y_base": round(120.0 * HALF, 3),
        "y_passo": round((cap_h_src + 10.0) * HALF, 3),
        # `FunkinSprite(0,0)` da capsula: o desenho parte do canto do canvas.
        "desenho_w": round(w * CAPSULA["real_scaled"], 3),
        "desenho_h": round(h * CAPSULA["real_scaled"], 3),
        # O titulo dentro da capsula, em 640x360.
        "texto_x": round(cw * CAPSULA["texto"]["x"] * CAPSULA["real_scaled"] * HALF, 3),
        "texto_y": round(CAPSULA["texto"]["y"] * CAPSULA["real_scaled"] * HALF, 3),
        "texto_clip": round(CAPSULA["texto"]["clip"] * CAPSULA["real_scaled"] * HALF, 3),
        "xframes": XFRAMES,
        "xpos_lerp": XPOS_LERP,
        "lerp_x": LERP_X,
        "lerp_y": LERP_Y,
        "nota": "Gerado por tools/empacotar_freeplay.py a partir de "
                "preload/images/freeplay do funkin.assets. Constantes e "
                "posicoes: SongMenuItem.hx / CapsuleText.hx v0.8.6. O header "
                "src/engine/freeplay_dados.h e' RENDERIZACAO deste arquivo.",
    }
    return dado, problemas, []


def escreve_header(dado):
    """O mesmo dado, em C, para o jogo nao ler JSON."""
    L = []
    a = L.append
    a("/* GENERATED by tools/empacotar_freeplay.py -- do not edit by hand.")
    a(" *")
    a(" * A arte da tela de FREEPLAY: a capsula (CFZ4, 16 quadros), o titulo de")
    a(" * cada musica (RGBA) e as constantes de posicao de `SongMenuItem.hx`.")
    a(" * Toda constante aqui tem linha na fonte v0.8.6; ver o cabecalho do")
    a(" * empacotador para os numeros e o porque de cada um.")
    a(" */")
    a("#ifndef FREEPLAY_DADOS_H")
    a("#define FREEPLAY_DADOS_H")
    a("")
    c = dado["capsula"]
    a("#define FP_CAP_W            %d      /* quadro na metade: %dx%d */"
      % (c["w"], c["src_w"], c["src_h"]))
    a("#define FP_CAP_H            %d" % c["h"])
    a("#define FP_CAP_QUADROS      %d" % c["quadros"])
    a("#define FP_CAP_SEL_INI      %d" % c["sel_ini"])
    a("#define FP_CAP_SEL_N        %d" % c["sel_n"])
    a("#define FP_CAP_NAO_INI      %d" % c["nao_ini"])
    a("#define FP_CAP_NAO_N        %d" % c["nao_n"])
    a("#define FP_CAP_FPS          %d" % c["fps"])
    a("#define FP_CAP_VRAM         %du     /* %d B por quadro */"
      % (c["vram_bytes"], c["vram_bytes"]))
    a("#define FP_CAP_TEX          \"%s\"    /* %s */"
      % (c["tex"], " + ".join(c["partes"])))
    a("")
    a("#define FP_X_BASE           %.3ff   /* 270 * 0,5 */" % dado["x_base"])
    a("#define FP_X_AMP            %.3ff   /* 60 * 0,5  */" % dado["x_amp"])
    a("#define FP_Y_BASE           %.3ff   /* 120 * 0,5 */" % dado["y_base"])
    a("#define FP_Y_PASSO          %.3ff   /* (capsule.height*0,8 + 10) * 0,5 */"
      % dado["y_passo"])
    a("#define FP_DESENHO_W        %.3ff" % dado["desenho_w"])
    a("#define FP_DESENHO_H        %.3ff" % dado["desenho_h"])
    a("#define FP_TEXTO_X          %.3ff   /* capsule.width * 0,26 */" % dado["texto_x"])
    a("#define FP_TEXTO_Y          %.3ff   /* 45, em 1280x720 */" % dado["texto_y"])
    a("#define FP_TEXTO_CLIP       %.3ff   /* CapsuleText.clipWidth = 255 */"
      % dado["texto_clip"])
    a("#define FP_LERP_X           %.3ff" % dado["lerp_x"])
    a("#define FP_LERP_Y           %.3ff" % dado["lerp_y"])
    a("#define FP_XFRAMES_N        %d" % len(dado["xframes"]))
    a("")
    a("static const float fp_xframes[FP_XFRAMES_N] = {%s};"
      % ",".join("%.2ff" % v for v in dado["xframes"]))
    a("static const float fp_xpos_lerp[] = {%s};"
      % ",".join("%.3ff" % v for v in dado["xpos_lerp"]))
    a("#define FP_XPOS_LERP_N      %d" % len(dado["xpos_lerp"]))
    a("")
    a("typedef struct {")
    a("    const char  *id;")
    a("    const char  *titulo;")
    a("    const char  *arquivo;      /* em assets/freeplay/ */")
    a("    unsigned int w, h;")
    a("    unsigned int vram_bytes;")
    a("} fp_titulo_t;")
    a("")
    a("static const fp_titulo_t fp_titulos[] = {")
    for sid in sorted(dado["titulos"]):
        t = dado["titulos"][sid]
        a("    {\"%s\", \"%s\", \"%s\", %du, %du, %du},"
          % (t["id"], t["titulo"].replace('"', '\\"'),
             os.path.basename(t["arquivo"]), t["w"], t["h"], t["vram_bytes"]))
    a("};")
    a("#define FP_TITULOS_N %d" % len(dado["titulos"]))
    a("")
    a("#endif /* FREEPLAY_DADOS_H */")
    texto = "\n".join(L) + "\n"
    with open(HEADER, "w", encoding="utf-8") as f:
        f.write(texto)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "freeplay"))
    ap.add_argument("--ver", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.ver:
        d = json.load(open(PLANO, encoding="utf-8"))
        c = d["capsula"]
        print("freeplay: capsula %dx%d, %d quadros (%d selecionados + %d nao), "
              "%d fps, %d B" % (c["w"], c["h"], c["quadros"], c["sel_n"],
                                c["nao_n"], c["fps"], c["vram_bytes"]))
        print("  passo vertical %.3f, x = %.1f +- %.1f, titulo em (%.1f, %.1f) "
              "cortado em %.1f px" % (d["y_passo"], d["x_base"], d["x_amp"],
                                      d["texto_x"], d["texto_y"], d["texto_clip"]))
        print("  %d titulos, o maior %d B"
              % (len(d["titulos"]),
                 max([t["vram_bytes"] for t in d["titulos"].values()] or [0])))
        return 0

    if args.check:
        d = json.load(open(PLANO, encoding="utf-8"))
        faltando = [p for p in d["capsula"]["partes"]
                    if not os.path.isfile(os.path.join(args.out, p))]
        if faltando:
            print("  SKIP: %d parte(s) do pacote ausentes em %s (rode --assets ...)"
                  % (len(faltando), os.path.relpath(args.out, PROJECT)))
            return 0
        if not os.path.isfile(HEADER):
            print("  FALHA: falta %s" % os.path.relpath(HEADER, PROJECT))
            return 1
        print("  ok:   capsula com %d partes, header em %s"
              % (len(d["capsula"]["partes"]), os.path.relpath(HEADER, PROJECT)))
        return 0

    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets> (ou --ver/--check)")
        return 1
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        print("  FALHA: falta o Pillow (PYTHONPATH=/tmp/pylibs)")
        return 1

    dado, problemas, _ = empacotar(args.assets, args.out)
    for p in problemas:
        print("  " + p)
    if not dado:
        print("  FALHA: nada empacotado")
        return 1

    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(dado, f, indent=1, ensure_ascii=False)
        f.write("\n")
    escreve_header(dado)

    c = dado["capsula"]
    print("freeplay: capsula %dx%d (%dx%d na arte), %d quadros, %d B por quadro"
          % (c["w"], c["h"], c["src_w"], c["src_h"], c["quadros"], c["vram_bytes"]))
    print("  %d titulos (%.1f px de fonte), o maior %d B"
          % (len(dado["titulos"]),
             CAPSULA["texto"]["tamanho"] * CAPSULA["real_scaled"] * HALF,
             max(t["vram_bytes"] for t in dado["titulos"].values())))
    print("  plano em assets-data/freeplay-plan.json, header em "
          "src/engine/freeplay_dados.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
