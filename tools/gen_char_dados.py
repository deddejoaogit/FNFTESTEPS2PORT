#!/usr/bin/env python3
"""Gera src/engine/char_dados.h: os 46 personagens e as animacoes de cada um.

POR QUE EXISTE
`src/bs_dados.h` so' tem offsets, cameraOffsets e singTime por personagem. O que
falta para desenhar qualquer um dos 46 e' a LISTA DE ANIMACOES: nome, ordem e o
prefixo no atlas de origem. Sem ela o port sabe que existe um "bf-christmas" e
nao sabe que animacoes ele tem -- e ficaria preso ao par bf/gf do tutorial,
cujas faixas de quadro foram descobertas a mao.

A fonte e' `assets-data/characters/*.json` (o CharacterData do v0.8.6), a mesma
que gerou o bs_dados.h. Gerar em vez de digitar tem o mesmo motivo de sempre:
nao pode divergir. E o teste confere justamente a divergencia possivel -- os
offsets/cameraOffsets/singTime daqui contra os do bs_dados.h, personagem por
personagem.

Uso:  python3 tools/gen_char_dados.py            (escreve o header)
      python3 tools/gen_char_dados.py --check    (falha se estiver velho)
"""
import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
SRC = os.path.join(PROJECT, "assets-data", "characters")
OUT = os.path.join(PROJECT, "src", "engine", "char_dados.h")

# renderType: o port so' sabe desenhar o que o empacotador empacotou, mas a
# informacao fica aqui porque ela decide QUANTOS spritemaps um personagem
# ocupa em VRAM e RAM (multianimateatlas pode ter varios).
CH_RENDER_MULTIANIMATEATLAS = 0
CH_RENDER_MULTISPARROW = 1
CH_RENDER_ANIMATEATLAS = 2
CH_RENDER_SPARROW = 3
CH_RENDER_PACKER = 4
# "packer" existe no original (CharacterData.hx L686, PackerCharacter) e e' o
# que o dad do fim de semana usa; sem ele o personagem sairia como custom.
RENDER = {
    "multianimateatlas": CH_RENDER_MULTIANIMATEATLAS,
    "multisparrow": CH_RENDER_MULTISPARROW,
    "animateatlas": CH_RENDER_ANIMATEATLAS,
    "sparrow": CH_RENDER_SPARROW,
    "packer": CH_RENDER_PACKER,
}
# character.json sem campo renderType NAO inventa nada: o CharacterData.hx
# (L463 DEFAULT_RENDERTYPE, L510-512) preenche "sparrow" antes de qualquer uso.
RENDER_DEFAULT = CH_RENDER_SPARROW


def c_str(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def c_float(v):
    return "%.6ff" % float(v)


def render(plan):
    linhas = []
    linhas.append("/* GERADO POR tools/gen_char_dados.py - NAO EDITAR A MAO.")
    linhas.append("   Fonte: assets-data/characters (um JSON por personagem,")
    linhas.append("   o CharacterData do v0.8.6),")
    linhas.append("   a mesma que gerou src/bs_dados.h. offsets/cameraOffsets/singTime")
    linhas.append("   tem de bater com o bs_dados.h, e test/test_char_layout.c")
    linhas.append("   compara os 46 contra os 46. */")
    linhas.append("#ifndef CHAR_DADOS_H")
    linhas.append("#define CHAR_DADOS_H")
    linhas.append("")
    linhas.append("/* Uma animacao de um personagem: o nome que o jogo usa (\"idle\",")
    linhas.append("   \"singLEFT\", \"singLEFTmiss\", \"danceLeft\") e o prefixo no atlas")
    linhas.append("   de origem (\"BF idle dance\"), que e' o que o empacotador procura. */")
    linhas.append("typedef struct{const char*name;const char*prefix;}ch_anim_dados_t;")
    linhas.append("")
    linhas.append("typedef struct{")
    linhas.append("  const char*id;")
    linhas.append("  int render;          /* CH_RENDER_* */")
    linhas.append("  int flipx;")
    linhas.append("  float ox,oy;         /* offsets */")
    linhas.append("  float cx,cy;         /* cameraOffsets */")
    linhas.append("  float sing;          /* singTime em passos; 0 = nao definido */")
    linhas.append("  float scale;         /* `scale` do CharacterData (1; 6 nos de pixel) */")
    linhas.append("  int anim_first,anim_count;")
    linhas.append("}ch_dados_t;")
    linhas.append("")
    linhas.append("#define CH_RENDER_MULTIANIMATEATLAS 0")
    linhas.append("#define CH_RENDER_MULTISPARROW     1")
    linhas.append("#define CH_RENDER_ANIMATEATLAS     2")
    linhas.append("#define CH_RENDER_SPARROW          3")
    linhas.append("#define CH_RENDER_PACKER           4")
    linhas.append("/* Personagem sem renderType no JSON vem como SPARROW")
    linhas.append("   (CharacterData.hx L463/L510-512, DEFAULT_RENDERTYPE). */")
    linhas.append("")

    anims = []
    chars = []
    for f in sorted(glob.glob(os.path.join(SRC, "*.json"))):
        d = json.load(open(f))
        cid = os.path.basename(f)[:-5]
        off = d.get("offsets") or [0.0, 0.0]
        cam = d.get("cameraOffsets") or [0.0, 0.0]
        sing = d.get("singTime")
        # `scale` do CharacterData: 1 por padrao, 6 nos personagens de pixel
        # (senpai, spirit, bf-pixel...). O original multiplica por ele em
        # CharacterData.DEFAULT_SCALE/Stage.addCharacter
        # (finalScale = getBaseScale() * stageCharData.scale), entao sem este
        # campo o personagem de pixel sai 6x menor do que o original desenha.
        esc = d.get("scale")
        esc = float(esc) if esc is not None else 1.0
        first = len(anims)
        for a in d.get("animations") or []:
            nome = a.get("name")
            if not nome:
                continue
            anims.append((nome, a.get("prefix") or nome))
        chars.append({
            "id": cid,
            "render": RENDER.get(d.get("renderType") or "", RENDER_DEFAULT),
            "flipx": 1 if d.get("flipX") else 0,
            "ox": float(off[0]), "oy": float(off[1]),
            "cx": float(cam[0]), "cy": float(cam[1]),
            "sing": float(sing) if sing is not None else 0.0,
            "scale": esc,
            "af": first, "ac": len(anims) - first,
        })

    linhas.append("/* Animacoes de todos os personagens, agrupadas por personagem. */")
    linhas.append("static const ch_anim_dados_t ch_anims[]={")
    for nome, pref in anims:
        linhas.append("  {%s,%s}," % (c_str(nome), c_str(pref)))
    linhas.append("};")
    linhas.append("#define CH_ANIM_COUNT %d" % len(anims))
    linhas.append("")
    linhas.append("static const ch_dados_t ch_chars[]={")
    for c in chars:
        linhas.append('  {%s,%d,%d,%s,%s,%s,%s,%s,%s,%d,%d},'
                      % (c_str(c["id"]), c["render"], c["flipx"],
                         c_float(c["ox"]), c_float(c["oy"]),
                         c_float(c["cx"]), c_float(c["cy"]), c_float(c["sing"]),
                         c_float(c["scale"]),
                         c["af"], c["ac"]))
    linhas.append("};")
    linhas.append("#define CH_CHAR_COUNT %d" % len(chars))
    linhas.append("")
    linhas.append("#endif /* CHAR_DADOS_H */")
    linhas.append("")
    return "\n".join(linhas), len(chars), len(anims)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv[1:])
    texto, nchars, nanims = render(None)
    if args.check:
        atual = open(OUT).read() if os.path.isfile(OUT) else ""
        if atual != texto:
            print("char_dados.h NAO corresponde a assets-data/characters; "
                  "rode tools/gen_char_dados.py")
            return 1
        print("char_dados.h confere com assets-data/characters")
        return 0
    with open(OUT, "w") as f:
        f.write(texto)
    print("wrote %s: %d personagens, %d animacoes"
          % (os.path.relpath(OUT, PROJECT), nchars, nanims))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
