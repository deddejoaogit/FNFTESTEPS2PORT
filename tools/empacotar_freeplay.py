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

# O FUNDO DA TELA (`FreeplayState.hx` L340, L456 e `BackingCard.hx` L53-62):
#
#     backingImage = FunkinSprite.create(backingCard.pinkBack.width * 0.74, 0,
#                                       styleData.getBgAssetKey());
#     backingImage.setGraphicSize(0, FlxG.height + 1);      // altura 721
#     ...
#     backingImage.x = -backingImage.width;                 // entra da esquerda
#     FlxTween.tween(backingImage, {x: 0}, 0.7, {ease: quintOut});
#
# E o `pinkBack` e' `BitmapUtil.scalePartByWidth(..., CUTOUT_WIDTH)` com
# `CUTOUT_WIDTH = gameCutoutSize.x / 1.5` = **0** no PS2 (nao ha' recorte de
# tela de aparelho), entao `pinkBack.width` e' 0 e o `create` sai em x = 0.
# Com `setGraphicSize(0, 721)` o flixel calcula a largura pela proporcao:
#
#     821 x 516  ->  altura 721  ->  largura 821 * 721 / 516 = 1147,11
#
# Em 640x360 (metade) isso e' 573,55 x 360, desenhado em (0,0). Nao ha'
# escolha de posicao a fazer: o x e' 0 e a altura e' a da tela.
FUNDO = {
    "fonte": "freeplay/freeplayBGweek1-bf",     # `getBgAssetKey()` do estilo "bf"
    "fonte_pico": "freeplay/freeplayBGweek1-pico",
    # `setGraphicSize(0, FlxG.height + 1)`: 720 + 1 na fonte, 360 aqui.
    "altura_src": 721.0,
    # 16 bpp (PSMCT16): o fundo ocupa a tela inteira e em 32 bpp seriam 826.560
    # B -- metade do pool da Freeplay. A regra do projeto e' imagem cede, jogo
    # nao; o caminho de upload de 16 bpp ja' e' o dos personagens.
    "bpp": 16,
}

# O SELO DE RANK na capsula (`SongMenuItem.hx` L821-901, classe `FreeplayRank`):
#
#     frames = Paths.getSparrowAtlas('freeplay/rankbadges');
#     animation.addByPrefix('PERFECT',     'PERFECT rank0',     24, false);
#     animation.addByPrefix('EXCELLENT',   'EXCELLENT rank0',   24, false);
#     animation.addByPrefix('GOOD',        'GOOD rank0',        24, false);
#     animation.addByPrefix('PERFECTSICK', 'PERFECT rank GOLD', 24, false);
#     animation.addByPrefix('GREAT',       'GREAT rank0',       24, false);
#     animation.addByPrefix('LOSS',        'LOSS rank0',        24, false);
#     blend = BlendMode.ADD;
#     scale.set(0.9, 0.9);
#
# Quem escolhe a animacao e' `ScoringRank.getFreeplayRankIconAsset()`
# (Scoring.hx L606-627): PERFECT_GOLD -> PERFECTSICK, SHIT -> LOSS, e o resto
# pelo proprio nome. Cada uma toca UMA vez (looped = false) e para no ultimo
# quadro; quem manda tocar e' `updateScoringRank()` no `refreshDisplay()` da
# capsula, e o rank vem do SAVE (`getSongRank`).
#
# O `switch` do `set_rank` (L834-857) NAO tem fallthrough no Haxe (o manual e'
# explicito: "Case body expressions never fall through") e o corpo de um `case`
# e' opcional, entao os `case` com so' comentario dentro sao NO-OP. O unico
# deslocamento que sobra em pe' e' `offset.y -= 8` no GOOD e no GREAT.
RANKSELO = {
    "fonte": "freeplay/rankbadges",
    "fps": 24,
    "escala_sprite": 0.9,      # FreeplayRank.new: scale.set(0.9, 0.9)
    # `ScoringRank.getFreeplayRankIconAsset()`, na ordem do `rank` do port
    # (0 LOSS/SHIT, 1 GOOD, 2 GREAT, 3 EXCELLENT, 4 PERFECT, 5 PERFECT_GOLD).
    "anims": [
        {"nome": "LOSS",        "prefixo": "LOSS rank0"},
        {"nome": "GOOD",        "prefixo": "GOOD rank0"},
        {"nome": "GREAT",       "prefixo": "GREAT rank0"},
        {"nome": "EXCELLENT",   "prefixo": "EXCELLENT rank0"},
        {"nome": "PERFECT",     "prefixo": "PERFECT rank0"},
        {"nome": "PERFECTSICK", "prefixo": "PERFECT rank GOLD"},
    ],
    # `set_rank`: GOOD e GREAT sobem 8 px (na fonte), em 1280x720.
    "offset_y": [0.0, -8.0, -8.0, 0.0, 0.0, 0.0],
    # `FreeplayRank(420, 41)`: no espaco do item, que o port ja' desenha em
    # 640x360 (o x/y do item NAO e' multiplicado por realScaled: quem escala
    # 0,8 e' o sprite da capsula, nao o grupo).
    "pos": (420.0, 41.0),
    "bpp": 16,
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

    # ---- 3. O FUNDO -----------------------------------------------------
    fundo_png = os.path.join(base, FUNDO["fonte"] + ".png")
    dado_fundo = None
    if not os.path.isfile(fundo_png):
        problemas.append("freeplay: falta o fundo %s" % fundo_png)
    else:
        src = Image.open(fundo_png).convert("RGBA")
        # `setGraphicSize(0, 721)` -> a altura manda e a largura vem da
        # proporcao; aqui tudo na metade.
        esc = (FUNDO["altura_src"] / float(src.height)) * HALF
        fw = max(1, int(round(src.width * esc)))
        fh = max(1, int(round(FUNDO["altura_src"] * HALF)))
        if fw > MAX_TEX or fh > MAX_TEX:
            problemas.append("freeplay: o fundo %dx%d passa do limite %d"
                             % (fw, fh, MAX_TEX))
        else:
            im = src.resize((fw, fh), Image.LANCZOS)
            bpp = FUNDO["bpp"]
            cru = pp.to_ps2_bytes(im, bpp)
            blocos = [lz4.block.compress(cru, store_size=False)]
            partes = pp.write_packs(blocos, fw * fh * (bpp // 8),
                                    os.path.join(out, "fundo.cfz4"))
            dado_fundo = {
                "fonte": FUNDO["fonte"],
                "src_w": src.width, "src_h": src.height,
                "tex": "freeplay/fundo.cfz4",
                "partes": [os.path.basename(p[0]) for p in partes],
                "w": fw, "h": fh,
                "bpp": bpp,
                "quadros": 1,
                "vram_bytes": fw * fh * (bpp // 8),
                # `backingImage` entra da esquerda: nasce em -width e vai para 0
                # em 7/10 s com quintOut (`FreeplayState.hx` L529).
                "x": 0.0, "y": 0.0,
                "entrada_ms": 700,
                "linha": "FreeplayState.hx L340, L456, L529 e BackingCard.hx L53-62",
            }

    # ---- 3b. O SELO DE RANK --------------------------------------------
    # Um pacote so', com as seis animacoes em sequencia. Todas no MESMO canvas
    # (o maior quadro de todos), alinhadas no canto superior esquerdo: no
    # flixel o sprite e' posicionado pelo canto do QUADRO, entao rank que e'
    # mais estreito fica com a sobra a' direita, nao centralizado.
    dado_selo = None
    rb_png = os.path.join(base, RANKSELO["fonte"] + ".png")
    rb_xml = os.path.join(base, RANKSELO["fonte"] + ".xml")
    if not (os.path.isfile(rb_png) and os.path.isfile(rb_xml)):
        problemas.append("freeplay: falta o selo de rank %s" % rb_png)
    else:
        q_rb = ler_xml(rb_xml)
        folha_rb = Image.open(rb_png).convert("RGBA")
        listas = []
        for an in RANKSELO["anims"]:
            nomes = quadros_da_folha(q_rb, an["prefixo"])
            if not nomes:
                problemas.append("freeplay: a animacao %s do selo nao apareceu"
                                 % an["nome"])
            listas.append(nomes)
        todos = [n for lst in listas for n in lst]
        if not todos:
            problemas.append("freeplay: nenhum quadro do selo")
        else:
            # A escala: a do sprite (0,9) vezes a metade da tela. O resultado e'
            # arredondado UMA vez, sobre o canvas comum, para todos os quadros
            # caírem no mesmo retangulo (o pacote exige tamanho fixo).
            esc = RANKSELO["escala_sprite"] * HALF
            cw = max(q_rb[n]["fw"] or q_rb[n]["w"] for n in todos)
            ch = max(q_rb[n]["fh"] or q_rb[n]["h"] for n in todos)
            w = max(1, int(round(cw * esc)))
            h = max(1, int(round(ch * esc)))
            if w > MAX_TEX or h > MAX_TEX:
                problemas.append("freeplay: o selo %dx%d passa do limite %d"
                                 % (w, h, MAX_TEX))
            else:
                blocos = []
                for n in todos:
                    q = q_rb[n]
                    pedaco = folha_rb.crop((q["x"], q["y"],
                                            q["x"] + q["w"], q["y"] + q["h"]))
                    tela = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
                    tela.paste(pedaco, (-q["fx"], -q["fy"]))
                    tela = tela.resize((w, h), Image.LANCZOS)
                    blocos.append(lz4.block.compress(
                        pp.to_ps2_bytes(tela, RANKSELO["bpp"]),
                        store_size=False))
                partes = pp.write_packs(blocos, w * h * (RANKSELO["bpp"] // 8),
                                        os.path.join(out, "rankbadges.cfz4"))
                anims = []
                ini = 0
                for k, (an, nomes) in enumerate(zip(RANKSELO["anims"], listas)):
                    anims.append({
                        "nome": an["nome"],
                        "ini": ini,
                        "n": len(nomes),
                        # `offset.y -= 8` so' no GOOD (1) e no GREAT (2); aqui
                        # ja' na metade da tela.
                        "off_y": RANKSELO["offset_y"][k] * HALF,
                    })
                    ini += len(nomes)
                dado_selo = {
                    "fonte": RANKSELO["fonte"],
                    "tex": "freeplay/rankbadges.cfz4",
                    "partes": [os.path.basename(p[0]) for p in partes],
                    "src_w": cw, "src_h": ch,
                    "w": w, "h": h,
                    "bpp": RANKSELO["bpp"],
                    "quadros": len(blocos),
                    "fps": RANKSELO["fps"],
                    "vram_bytes": w * h * (RANKSELO["bpp"] // 8),
                    "anims": anims,
                    "x": round(RANKSELO["pos"][0] * HALF, 3),
                    "y": round(RANKSELO["pos"][1] * HALF, 3),
                    "linha": "SongMenuItem.hx L821-901 (FreeplayRank) e "
                             "Scoring.hx L606-627",
                }

    # ---- 4. O DADO -----------------------------------------------------
    # As posicoes saem de `SongMenuItem.intendedX/intendedY` (L762-769) e o
    # `capsule.height` do original e' o canvas SEM CORTE da CAPSULA (src_h), nao
    # o pedaco nem o selo.
    #
    # Vem do dado da capsula, e nao de variavel solta: a primeira versao usava
    # os `cw,ch,w,h` que sobravam do laco do SELO DE RANK, entao o passo
    # vertical saia 26,6 (metade de 54*0,8+10, a altura do selo) em vez de 57,8
    # e a capsula era desenhada com 18,4x19,2 px -- o tamanho do selo. O
    # `capsule.height` da fonte vale 132 e o `scale` 0,8 (`realScaled` L619,
    # aplicado em L669-670): 132*0,8 = 105,6; +10 = 115,6; metade = 57,8.
    cap_w_src = float(dado_capsula["src_w"]) * CAPSULA["real_scaled"]
    cap_h_src = float(dado_capsula["src_h"]) * CAPSULA["real_scaled"]
    dado = {
        "capsula": dado_capsula,
        "fundo": dado_fundo,
        "selo": dado_selo,
        "titulos": titulos_out,
        "escala": HALF,
        # `intendedX(index) = 270 + 60*fastSin(index)`; `intendedY(index) =
        # index*((capsule.height*realScaled) + 10) + 120`.
        "x_base": round(270.0 * HALF, 3),
        "x_amp": round(60.0 * HALF, 3),
        "y_base": round(120.0 * HALF, 3),
        "y_passo": round((cap_h_src + 10.0) * HALF, 3),
        # `FunkinSprite(0,0)` da capsula: o desenho parte do canto do canvas.
        "desenho_w": round(cap_w_src * HALF, 3),
        "desenho_h": round(cap_h_src * HALF, 3),
        # O titulo dentro da capsula, em 640x360.
        # `CapsuleText(capsule.width * 0,26, 45, ...)` (L191): a largura e' a da
        # CAPSULA (612), nao a de outro dado que esteja por perto.
        "texto_x": round(float(dado_capsula["src_w"]) * CAPSULA["texto"]["x"]
                         * CAPSULA["real_scaled"] * HALF, 3),
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
    f = dado.get("fundo")
    if f:
        a("/* O FUNDO (`backingImage`): `freeplayBGweek1-bf` a altura da tela")
        a(" * (720+1 na fonte) e em 16 bpp, desenhado em (0,0). */")
        a("#define FP_FUNDO_W          %d      /* %dx%d na arte */"
          % (f["w"], f["src_w"], f["src_h"]))
        a("#define FP_FUNDO_H          %d" % f["h"])
        a("#define FP_FUNDO_BPP        %d" % f["bpp"])
        a("#define FP_FUNDO_VRAM       %du" % f["vram_bytes"])
        a("#define FP_FUNDO_TEX        \"%s\"" % f["tex"])
        a("#define FP_FUNDO_ENTRADA_MS %d" % f["entrada_ms"])
        a("#define FP_FUNDO_X          %.3ff" % f["x"])
        a("#define FP_FUNDO_Y          %.3ff" % f["y"])
        a("")
    rb = dado.get("selo")
    if rb:
        a("/* O SELO DE RANK da capsula (`SongMenuItem.hx` L821-901): as seis")
        a(" * animacoes de `freeplay/rankbadges`, uma vez cada, no ultimo quadro.")
        a(" * `off_y` ja' na metade; so' GOOD e GREAT deslocam (`offset.y -= 8`). */")
        a("#define FP_RB_W             %d      /* %dx%d na arte */"
          % (rb["w"], rb["src_w"], rb["src_h"]))
        a("#define FP_RB_H             %d" % rb["h"])
        a("#define FP_RB_BPP           %d" % rb["bpp"])
        a("#define FP_RB_QUADROS       %d" % rb["quadros"])
        a("#define FP_RB_FPS           %d" % rb["fps"])
        a("#define FP_RB_VRAM          %du" % rb["vram_bytes"])
        a("#define FP_RB_TEX           \"%s\"" % rb["tex"])
        a("#define FP_RB_X             %.3ff   /* FreeplayRank(420, 41) */"
          % rb["x"])
        a("#define FP_RB_Y             %.3ff" % rb["y"])
        a("#define FP_RB_ANIMS_N       %d" % len(rb["anims"]))
        a("")
        a("typedef struct {")
        a("    const char *nome;   /* getFreeplayRankIconAsset() */")
        a("    int ini, n;         /* quadro inicial e quantos              */")
        a("    float off_y;        /* `offset.y -= 8` no GOOD e no GREAT    */")
        a("} fp_rb_anim_t;")
        a("")
        a("/* Na ordem do rank do port: 0 LOSS/SHIT, 1 GOOD, 2 GREAT,")
        a(" * 3 EXCELLENT, 4 PERFECT, 5 PERFECT_GOLD (a arte dourada). */")
        a("static const fp_rb_anim_t fp_rb_anims[FP_RB_ANIMS_N] = {")
        for an in rb["anims"]:
            a("    {\"%s\", %d, %d, %.1ff},"
              % (an["nome"], an["ini"], an["n"], an["off_y"]))
        a("};")
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
    fundo = dado.get("fundo")
    if fundo:
        print("freeplay: fundo %dx%d (%dx%d na arte, %d bpp), %d B"
              % (fundo["w"], fundo["h"], fundo["src_w"], fundo["src_h"],
                 fundo["bpp"], fundo["vram_bytes"]))
    else:
        print("freeplay: SEM FUNDO (ver os avisos)")
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
