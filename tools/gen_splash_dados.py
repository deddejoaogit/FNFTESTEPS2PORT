#!/usr/bin/env python3
"""Gera src/engine/splash_dados.h a partir de assets-data/splash-plan.json.

O header NAO e' escrito a mao: ele e' a renderizacao do plano, e o plano e' o
que `tools/empacotar_splash.py` viu nos pixels do noteSplashes.png. Foi assim
que o port deixou de ter tabelas digitadas que ninguem conseguia conferir --
`stage_dados.h`, `song_registry_data.inc` e `notekind_dados.h` seguem a mesma
regra.

O `--check` compara o texto gerado com o arquivo em disco: se o plano mudar (ou
alguem editar o header), a suite reprova antes de o console ver o erro.

Uso:
    tools/gen_splash_dados.py            # escreve o header
    tools/gen_splash_dados.py --check    # so' confere
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "splash-plan.json")
SAIDA = os.path.join(PROJECT, "src", "engine", "splash_dados.h")


def gerar():
    d = json.load(open(PLANO, encoding="utf-8"))
    r = d["regiao"]
    anims = d["animacoes"]
    faixa = d["faixa"]

    out = []
    a = out.append
    a("/* GERADO POR tools/gen_splash_dados.py -- NAO EDITE.")
    a(" *")
    a(" * Fonte: assets-data/splash-plan.json, que tools/empacotar_splash.py")
    a(" * escreve depois de recortar os 32 quadros de shared/images/noteSplashes")
    a(" * .png+xml. Os numeros abaixo sao do plano, nao digitos escolhidos.")
    a(" *")
    a(" * A REGIAO vive DENTRO do atlas do HUD (tex2, %dx%d PSMCT32), na faixa"
      % (d["tex_w"], d["tex_h"]))
    a(" * livre abaixo da barra de vida: VRAM nova = %d B. O comentario no topo"
      % d["vram_nova"])
    a(" * de src/engine/splash.c explica a regra do original.")
    a(" */")
    a("#ifndef SPLASH_DADOS_H")
    a("#define SPLASH_DADOS_H")
    a("")
    a("#define SPLASH_TEX_W %du" % d["tex_w"])
    a("#define SPLASH_TEX_H %du" % d["tex_h"])
    a("#define SPLASH_REG_X %du" % r["x"])
    a("#define SPLASH_REG_Y %du" % r["y"])
    a("#define SPLASH_REG_W %du" % r["w"])
    a("#define SPLASH_REG_H %du" % r["h"])
    a("#define SPLASH_ANIMS %du" % len(anims))
    a("#define SPLASH_QUADROS %du" % anims[0]["quadros"])
    a("/* Linhas do atlas que pertencem a UMA animacao (um quadro por linha). */")
    a("#define SPLASH_FAIXA %du" % faixa)
    a("")
    a("/* NoteSplash.hx L13-14: splashFramerate = 24, splashFramerateVariance = 2. */")
    a("#define SPLASH_FPS %du" % d["fps"])
    a("#define SPLASH_FPS_VAR %du" % d["fps_variancia"])
    a("/* preload/data/notestyles/funkin.json: assets.noteSplash.alpha = 0.8. */")
    a("#define SPLASH_ALPHA %du" % int(round(d["alpha"] * 255.0)))
    a("")
    a("/* Posicao. Strumline.playNoteSplash L1086-1094 e NoteSplash.play L56. */")
    a("#define SPLASH_OFF_X (%.1ff)" % d["offset_x"])
    a("#define SPLASH_OFF_Y (%.1ff)" % d["offset_y"])
    a("/* INITIAL_OFFSET = -0.275 * STRUMLINE_SIZE (104). */")
    a("#define SPLASH_INITIAL_OFFSET (%.4ff)" % d["initial_offset"])
    a("/* offset.set(width * 0.3, height * 0.3), subtraido no desenho. */")
    a("#define SPLASH_CENTRO (%.1ff)" % d["centro"])
    a("")
    a("typedef struct {")
    a("    unsigned short cx, cy, cw, ch;  /* celula no atlas (tex2)        */")
    a("    unsigned short lw, lh;          /* canvas logico do original     */")
    a("    float dw, dh;                   /* tamanho desenhado (640x360)   */")
    a("} nspl_cel_t;")
    a("")
    a("static const nspl_cel_t g_splash_cel[SPLASH_ANIMS] = {")
    for x in anims:
        a('    { %3du, %3du, %2du, %2du, %3du, %3du, %.1ff, %.1ff },  /* %s */'
          % (x["cx"], x["cy"], x["cw"], x["ch"], x["lw"], x["lh"],
             x["disp_w"], x["disp_h"], x["nome"]))
    a("};")
    a("")
    a("#endif /* SPLASH_DADOS_H */")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    texto = gerar()
    if args.check:
        if not os.path.isfile(SAIDA):
            print("  FALHA: %s nao existe -- rode tools/gen_splash_dados.py"
                  % os.path.relpath(SAIDA, PROJECT))
            return 1
        atual = open(SAIDA, encoding="utf-8").read()
        if atual != texto:
            print("  FALHA: src/engine/splash_dados.h nao e' a renderizacao do "
                  "plano (rode tools/gen_splash_dados.py)")
            return 1
        print("  ok:   splash_dados.h confere com assets-data/splash-plan.json")
        return 0
    with open(SAIDA, "w", encoding="utf-8") as f:
        f.write(texto)
    print("escrito %s" % os.path.relpath(SAIDA, PROJECT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
