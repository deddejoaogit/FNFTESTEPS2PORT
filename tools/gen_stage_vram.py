#!/usr/bin/env python3
"""Gera src/engine/stage_vram_data.h: quantos bytes de VRAM cada prop pede.

POR QUE EXISTE
O pool de texturas do palco (`src/engine/stage_pool.c`) precisa saber, para
cada um dos 157 props, quantos bytes alocar. Isso sai do plano commitado
(`assets-data/stage-props.json`): `sw*sh*2` (16 bpp), arredondado para 256
bytes, que e' o alinhamento do TBP0 do GS.

Gerar em vez de digitar a mao tem duas consequencias boas, e as duas ja'
importaram neste projeto:
  * o numero nao pode divergir do pacote (mesma fonte que gera o
    `stage_dados.h`);
  * o teste do pool roda no PC com a demanda REAL dos 21 palcos, e nao com
    tamanhos inventados -- foi assim que se descobriu que o plano atual nao
    cabe na VRAM livre.

Uso:  python3 tools/gen_stage_vram.py            (escreve o header)
      python3 tools/gen_stage_vram.py --check    (falha se estiver desatualizado)
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLAN = os.path.join(PROJECT, "assets-data", "stage-props.json")
OUT = os.path.join(PROJECT, "src", "engine", "stage_vram_data.h")
BPP = 16
ALINHAMENTO = 256   # VC_ALINHAMENTO: mesmo valor do vram_cache.c


def alinhar(v):
    return (v + ALINHAMENTO - 1) & ~(ALINHAMENTO - 1)


def render(plan):
    linhas = []
    linhas.append("/* GERADO POR tools/gen_stage_vram.py - NAO EDITAR A MAO.")
    linhas.append("   Fonte: assets-data/stage-props.json (o mesmo plano que gera")
    linhas.append("   src/stage_dados.h). Cada valor e' sw*sh*2 arredondado para 256")
    linhas.append("   bytes, que e' o alinhamento do TBP0 do GS (VC_ALINHAMENTO).")
    linhas.append("   O indice de cada linha e' o MESMO de bs_props[]. */")
    linhas.append("#ifndef STAGE_VRAM_DATA_H")
    linhas.append("#define STAGE_VRAM_DATA_H")
    linhas.append("")
    total = 0
    por_palco = []
    valores = []
    for st in plan["stages"]:
        soma = 0
        for p in st["props"]:
            b = alinhar(p["sw"] * p["sh"] * (BPP // 8))
            valores.append(b)
            soma += b
        por_palco.append((st["id"], soma))
        total += soma
    linhas.append("/* Bytes de VRAM de UM quadro de cada prop, na ordem de bs_props[]. */")
    linhas.append("static const unsigned int bs_prop_vram_bytes[] = {")
    for i in range(0, len(valores), 6):
        linhas.append("  " + " ".join("%5du," % v for v in valores[i:i + 6]))
    linhas.append("};")
    linhas.append("#define BS_PROP_VRAM_COUNT %d" % len(valores))
    linhas.append("")
    linhas.append("/* Soma por palco, na ordem de bs_stage_props[]. Serve para o teste")
    linhas.append("   saber, sem varrer o array, quanto cada palco pede. */")
    linhas.append("static const unsigned int bs_stage_vram_bytes[] = {")
    for i in range(0, len(por_palco), 4):
        linhas.append("  " + " ".join("%8du," % v for _n, v in por_palco[i:i + 4]))
    linhas.append("};")
    linhas.append("#define BS_STAGE_VRAM_COUNT %d" % len(por_palco))
    linhas.append("")
    linhas.append("/* Pior palco do plano atual: %s. */" % max(por_palco, key=lambda t: t[1])[0])
    linhas.append("#define BS_STAGE_VRAM_MAX %du" % max(v for _n, v in por_palco))
    linhas.append("")
    linhas.append("#endif /* STAGE_VRAM_DATA_H */")
    linhas.append("")
    return "\n".join(linhas), por_palco


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv[1:])
    plan = json.load(open(PLAN))
    texto, por_palco = render(plan)
    if args.check:
        atual = open(OUT).read() if os.path.isfile(OUT) else ""
        if atual != texto:
            print("stage_vram_data.h NAO corresponde ao plano; rode tools/gen_stage_vram.py")
            return 1
        print("stage_vram_data.h confere com o plano")
        return 0
    with open(OUT, "w") as f:
        f.write(texto)
    pior = max(por_palco, key=lambda t: t[1])
    print("wrote %s: %d props, %d palcos, pior %s com %d bytes"
          % (os.path.relpath(OUT, PROJECT), len(plan["stages"]) and
             sum(len(s["props"]) for s in plan["stages"]), len(por_palco), pior[0], pior[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
