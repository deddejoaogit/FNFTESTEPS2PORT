#!/usr/bin/env python3
"""Gera src/engine/resultado_dados.h a partir de assets-data/resultado-plan.json.

Mesma regra dos outros geradores do projeto: o header em C e' a RENDERIZACAO do
plano, e o plano e' o que o empacotador viu nos pixels. Nenhum numero de
posicao, tamanho ou tempo e' digitado a mao.

O `--check` compara o texto gerado com o arquivo em disco, entao a suite reprova
se o plano mudar sem o header -- ou se alguem editar o header.

Uso:
    tools/gen_resultado_dados.py            # escreve o header
    tools/gen_resultado_dados.py --check    # so' confere
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")
SAIDA = os.path.join(PROJECT, "src", "engine", "resultado_dados.h")


def gerar():
    d = json.load(open(PLANO, encoding="utf-8"))
    folhas = d["folhas"]
    out = []
    a = out.append
    a("/* GERADO POR tools/gen_resultado_dados.py -- NAO EDITE.")
    a(" *")
    a(" * Fonte: assets-data/resultado-plan.json, escrito por")
    a(" * tools/empacotar_resultado.py depois de recortar as folhas de")
    a(" * shared/images/resultScreen. Posicoes, tempos e fps vem de")
    a(" * source/funkin/play/ResultState.hx (v0.8.6); as linhas estao no plano.")
    a(" */")
    a("#ifndef RESULTADO_DADOS_H")
    a("#define RESULTADO_DADOS_H")
    a("")
    a("/* Escala: o original monta a tela em 1280x720, o port desenha em 640x360. */")
    a("#define RES_ESCALA (%.1ff)" % d["escala"])
    a("#define RES_FOLHAS %du" % len(folhas))
    a("")
    a("/* O fundo: FlxGradient de 90 graus entre estas duas cores. */")
    a("#define RES_BG_W %du" % d["bg_w"])
    a("#define RES_BG_H %du" % d["bg_h"])
    a("#define RES_BG_TOPO_R %du" % d["bg_topo"][0])
    a("#define RES_BG_TOPO_G %du" % d["bg_topo"][1])
    a("#define RES_BG_TOPO_B %du" % d["bg_topo"][2])
    a("#define RES_BG_BASE_R %du" % d["bg_base"][0])
    a("#define RES_BG_BASE_G %du" % d["bg_base"][1])
    a("#define RES_BG_BASE_B %du" % d["bg_base"][2])
    a("")
    a("typedef struct {")
    a("    unsigned short w, h;        /* canvas da folha em 640x360       */")
    a("    unsigned short quadros;     /* quadros da animacao              */")
    a("    unsigned short fps;")
    a("    unsigned char  looped;")
    a("    float          x, y;        /* canto superior esquerdo na tela  */")
    a("    unsigned int   inicio_ms;   /* quando a folha aparece           */")
    a("    const char    *tex;         /* caminho do pacote CFZ4           */")
    a("    const char    *id;")
    a("} res_folha_dado_t;")
    a("")
    a("static const res_folha_dado_t g_res_folha[RES_FOLHAS] = {")
    for f in folhas:
        a('    { %3du, %3du, %2du, %2du, %du, %.1ff, %.1ff, %4du, "%s", "%s" },'
          % (f["w"], f["h"], f["quadros"], f["fps"], 1 if f["looped"] else 0,
             f["x"], f["y"], f["inicio_ms"], f["tex"], f["id"]))
    a("};")
    a("")
    a("#endif /* RESULTADO_DADOS_H */")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    texto = gerar()
    if args.check:
        if not os.path.isfile(SAIDA):
            print("  FALHA: %s nao existe -- rode tools/gen_resultado_dados.py"
                  % os.path.relpath(SAIDA, PROJECT))
            return 1
        if open(SAIDA, encoding="utf-8").read() != texto:
            print("  FALHA: src/engine/resultado_dados.h nao e' a renderizacao do "
                  "plano (rode tools/gen_resultado_dados.py)")
            return 1
        print("  ok:   resultado_dados.h confere com assets-data/resultado-plan.json")
        return 0
    with open(SAIDA, "w", encoding="utf-8") as f:
        f.write(texto)
    print("escrito %s" % os.path.relpath(SAIDA, PROJECT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
