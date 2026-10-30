#!/usr/bin/env python3
"""Conferência offline do orçamento de TODOS os palcos, contra o plano commitado.

POR QUE EXISTE
O empacotador (`tools/pacotar_palcos.py`) precisa do clone de 2,5 GB da arte
para gerar os .cfz4, e por isso não roda aqui. Mas o PLANO que ele escreveu
(`assets-data/stage-props.json`) está no repositório e tem tudo que a conta
precisa: tamanho de um quadro (sw/sh), número de quadros e a escala usada.

Este script lê o plano e responde duas perguntas, com números:

  1. VRAM: um quadro de cada prop do palco cabe na região de textura?
     (a VRAM paga UM quadro por prop; o pacote fica na RAM)
  2. RAM: o pacote inteiro do palco cabe na RAM principal?
     (a RAM paga TODOS os quadros de TODOS os props)

A segunda pergunta nunca tinha sido feita: o empacotador dimensionou tudo por
VRAM. Foi por isso que o `spookyMansion` saiu com 59 MB de pacote num console de
32 MiB -- e nada no repositório reclamava.

Uso:
    python3 tools/check_stage_budget.py            # relatório, sai 0
    python3 tools/check_stage_budget.py --strict   # falha se algum palco estourar

O modo estrito é o que vale DEPOIS do próximo empacotamento. Antes dele o
relatório mostra a dívida medida, com a escala que cada palco teria de usar.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import vram_budget as vb

PLAN = os.path.join(PROJECT, "assets-data", "stage-props.json")
LOG = os.path.join(PROJECT, "build", "harness_assets", "FNFPS2", "FNF_COLHEITA.txt")

BPP = 16
MAX_TEX = 1024   # o GS nao amostra textura mais larga que isso


def measured_vram_free():
    """`vram_livre` do último log do harness, em bytes. None se não houver."""
    if not os.path.isfile(LOG):
        return None
    free = None
    with open(LOG, "r", errors="replace") as f:
        for line in f:
            if ",vram_livre," in line:
                try:
                    free = int(line.rsplit(",", 2)[-2])
                except (ValueError, IndexError):
                    pass
    return free


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--ram-elF", dest="elf", type=int, default=vb.ELF_IMAGE_BYTES)
    args = ap.parse_args(argv[1:])

    if not os.path.isfile(PLAN):
        print("FALTA %s (rode tools/pacotar_palcos.py)" % PLAN)
        return 2
    plan = json.load(open(PLAN))

    vram_model = vb.ram_limit_per_stage  # so' para deixar claro que existe
    del vram_model
    fb = vb.FB_W * vb.FB_H * (vb.FB_PSM_BITS // 8)
    fixed = (vb.FONT_W * vb.FONT_H * 4 + vb.HUD_W * vb.HUD_H * (BPP // 8)
             + vb.NOTE_W * vb.NOTE_H * (BPP // 8))
    vram_budget = vb.VRAM_TOTAL - vb.FB_COUNT * fb - fixed - vb.char_frame_bytes(BPP)
    vram_medido = measured_vram_free()
    ram_budget = vb.ram_limit_per_stage(elf_bytes=args.elf)

    print("BPP %d | framebuffers %d x %dx%d PSMCT%d = %d B"
          % (BPP, vb.FB_COUNT, vb.FB_W, vb.FB_H, vb.FB_PSM_BITS, vb.FB_COUNT * fb))
    print("VRAM para o palco: modelo %d B | medido no ultimo log %s"
          % (vram_budget, ("%d B" % vram_medido) if vram_medido else "ausente"))
    print("RAM  para o pacote do palco: %d B  (32 MiB - ELF %d - buffers %d - staging %d)"
          % (ram_budget, args.elf, vb.RAM_BUFFERS_BYTES, vb.RAM_STAGING_BYTES))
    print()
    print("%-26s %6s %7s %11s %11s %s"
          % ("palco", "props", "escala", "VRAM 1 quadro", "RAM pacote", "situação"))
    print("-" * 96)

    over_vram, over_ram, over_tex = [], [], []
    for st in plan["stages"]:
        sid = st["id"]
        props = st["props"]
        vram = sum(p["sw"] * p["sh"] * (BPP // 8) for p in props)
        ram = vb.stage_ram_bytes(props)
        notas = []
        if vram > vram_budget:
            over_vram.append(sid)
            notas.append("VRAM +%d B" % (vram - vram_budget))
        if ram > ram_budget:
            over_ram.append(sid)
            notas.append("RAM +%d B" % (ram - ram_budget))
        for p in props:
            if p["sw"] > MAX_TEX or p["sh"] > MAX_TEX:
                over_tex.append("%s/%s %dx%d" % (sid, p["name"], p["sw"], p["sh"]))
        print("%-26s %6d %7.3f %11d %11d %s"
              % (sid, len(props), st["scale"], vram, ram,
                 ", ".join(notas) if notas else "ok"))

    print()
    if over_tex:
        print("FALHA: prop acima do limite de %d px do GS: %s"
              % (MAX_TEX, ", ".join(over_tex[:4])))
    if over_vram:
        print("VRAM: %d de %d palcos acima do orçamento -- %s"
              % (len(over_vram), len(plan["stages"]), ", ".join(over_vram[:6])))
    if over_ram:
        print("RAM: %d de %d palcos acima do orçamento -- %s"
              % (len(over_ram), len(plan["stages"]), ", ".join(over_ram[:6])))

    # A escala que cada palco teria de usar, para o relatório ser acionável.
    if over_ram or over_vram:
        print()
        print("Escala que fecharia o orçamento (recortar o plano e reempacotar):")
        for st in plan["stages"]:
            sid = st["id"]
            props = st["props"]
            if sid not in over_vram and sid not in over_ram:
                continue
            nat = [(p["sw"] / st["scale"], p["sh"] / st["scale"],
                    p["frames"]) for p in props]
            f_vram = _scale(nat, vram_budget, per_frame=True)
            f_ram = _scale(nat, ram_budget, per_frame=False)
            print("  %-26s agora %.3f -> VRAM %.3f / RAM %.3f  => usar %.3f"
                  % (sid, st["scale"], f_vram, f_ram, min(f_vram, f_ram)))

    if over_tex:
        return 1
    if args.strict and (over_vram or over_ram):
        return 1
    print()
    print("plano conferido: %d palcos, %d props"
          % (len(plan["stages"]), sum(len(s["props"]) for s in plan["stages"])))
    return 0


def _scale(nat, budget, per_frame):
    """Maior escala uniforme que cabe em `budget` (VRAM: 1 quadro; RAM: todos)."""
    def total(f):
        s = 0
        for w, h, n in nat:
            px = max(1, int(round(w * f))) * max(1, int(round(h * f)))
            s += px * (BPP // 8) * (1 if per_frame else n)
        return s
    if total(1.0) <= budget:
        return 1.0
    lo, hi = 0.0001, 1.0
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if total(mid) <= budget:
            lo = mid
        else:
            hi = mid
    return lo


if __name__ == "__main__":
    sys.exit(main(sys.argv))
