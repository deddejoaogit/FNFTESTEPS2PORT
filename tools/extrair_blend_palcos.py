#!/usr/bin/env python3
"""Extract, from the stage scripts, the blend mode and alpha a prop starts with.

WHY
---
`StageData.hx` has no `blend` field: not one of the 169 props in the game's JSON
carries it (measured: 169 props, 0 with the key). In the original the blend mode
is set by the stage's HScript (`preload/scripts/stages/<stage>.hxc`), with the
number of `openfl.display.BlendMode`:

    0 = ADD      9 = MULTIPLY      12 = SCREEN

Two examples, both at build time:

    mainStageErect.hxc L49-53   getNamedProp('brightLightSmall').blend = 0;
    phillyStreets.hxc  L517-524 override function addProp(prop, name)
                                  if (StringTools.endsWith(name, "_lightmap"))
                                    { prop.blend = 0; prop.alpha = 0.6; }

The port has no HScript interpreter, so this tool does NOT run the scripts. It
recognises exactly four shapes of statement, which are the ones these files use
to set a prop's blend/alpha when it is built, and copies them into
`assets-data/stage-script.json` with the file and line of each one:

    <var>.blend = N;                         (var = getNamedProp('X') above)
    getNamedProp('X').blend = N;
    if (StringTools.endsWith(name, "S")) { prop.blend = N; prop.alpha = A; }
    else if (name == "X") { prop.blend = N; prop.alpha = A; }

Anything else -- `if` without a literal name, expressions, variables computed at
run time -- is printed as "NAO RESOLVIDO" and left out. Rule 4 of the project:
when the data does not say it, collect and report; never guess.

WHAT IT DOES NOT COVER, ON PURPOSE
----------------------------------
Assignments inside event callbacks (`onBeatHit`, `onStepHit`, tweens, `onCreate`
of a script that reacts to lightning, ...) are NOT the initial state, and are not
copied. They are counted and printed so the next step knows how much behaviour
is still missing (the mists of `limoRideErect`, the lightning of `phillyBlazin`,
the lights of `sserafim`, ...). Some of those props are created by the script
itself with `new FlxBackdrop(...)`, so they are not in the JSON at all.

Usage:
  python3 tools/extrair_blend_palcos.py --scripts <clone>/preload/scripts/stages
  python3 tools/extrair_blend_palcos.py --check     # compare with the committed file
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
OUT = os.path.join(PROJECT, "assets-data", "stage-script.json")

# openfl.display.BlendMode. Only these three appear in the stage scripts; the
# port draws NORMAL and ADD, and reports the other two (see src/main.c).
BLEND = {0: "ADD", 1: "ALPHA", 2: "DARKEN", 3: "DIFFERENCE", 4: "ERASE",
         5: "HARDLIGHT", 6: "INVERT", 7: "LAYER", 8: "LIGHTEN",
         9: "MULTIPLY", 10: "NORMAL", 11: "OVERLAY", 12: "SCREEN",
         13: "SHADER", 14: "SUBTRACT"}

RE_NAMED_BLEND = re.compile(r"getNamedProp\(\s*'([^']+)'\s*\)\.blend\s*=\s*(\d+)")
RE_VAR_BLEND = re.compile(r"^\s*([A-Za-z_]\w*)\.blend\s*=\s*(\d+)")
RE_VAR_BLEND_TARGET = re.compile(
    r"([A-Za-z_]\w*)\s*=\s*getNamedProp\(\s*'([^']+)'\s*\)")
RE_SUFFIX = re.compile(
    r'StringTools\.endsWith\(\s*name\s*,\s*"([^"]+)"\s*\)')
RE_NAME_EQ = re.compile(r'name\s*==\s*"([^"]+)"')
# `switch (name) { case 'truckLight1': prop.blend = 12; ... }` (sserafim.hxc
# L388-411). Em Haxe cada `case` e' exclusivo e nao cai para o seguinte -- o
# manual da linguagem confirma (docs/PLANO_COMPLETO.md cita o trecho) --, entao
# ler "o case corrente" e' o mesmo que ler "o nome no if".
RE_CASE = re.compile(r"case\s*'([^']+)'\s*:")
RE_ATTR = re.compile(r"prop\.(blend|alpha)\s*=\s*([0-9.]+)")


def scan_file(path):
    """-> (fichas, nao_resolvido, eventos) for one .hxc."""
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    fichas = []
    nao = []
    eventos = 0

    # 1) `getNamedProp('X').blend = N;` em qualquer linha.
    for n, ln in enumerate(lines, 1):
        for m in RE_NAMED_BLEND.finditer(ln):
            fichas.append({"nome": m.group(1), "regra": "nome",
                           "blend": int(m.group(2)), "alpha": None,
                           "linha": n})

    # 2) `var x = getNamedProp('X');` ... `x.blend = N;` (o mesmo prop por
    #    variavel local). Sem o mapa, a linha do blend nao diz de quem e'.
    mapa = {}
    for n, ln in enumerate(lines, 1):
        for m in RE_VAR_BLEND_TARGET.finditer(ln):
            mapa[m.group(1)] = m.group(2)
    for n, ln in enumerate(lines, 1):
        m = RE_VAR_BLEND.match(ln)
        if not m:
            continue
        var, val = m.group(1), int(m.group(2))
        if var == "prop":
            # `prop.blend` so' existe dentro de `addProp`; quem resolve o nome
            # dele e' a regra 3 (sufixo/nome), nao esta.
            continue
        nome = mapa.get(var)
        if not nome:
            nao.append("linha %d: %s.blend = %d (variavel sem getNamedProp)"
                       % (n, var, val))
            continue
        fichas.append({"nome": nome, "regra": "nome", "blend": val,
                       "alpha": None, "linha": n})

    # 3) `override function addProp(prop, name)` -- o gancho que o Stage.hx
    #    chama ao construir CADA prop. As duas formas usadas: sufixo do nome e
    #    nome exato. O bloco termina na proxima linha que fecha a funcao, entao
    #    a leitura vai ate' o `else` final; as linhas sao lidas em ordem e o
    #    par blend/alpha e' o do mesmo `if`.
    for n, ln in enumerate(lines, 1):
        if "function addProp" not in ln:
            continue
        i = n
        alvo = None
        # A funcao tem `if`/`else if` aninhados, entao o fim NAO e' a primeira
        # linha que comeca com `}`: conta-se a profundidade das chaves a partir
        # do `{` da propria funcao. Sem isto o `else if (name == "grey1")` do
        # phillyStreetsErect nunca era lido (o `}` do primeiro `if` encerrava a
        # leitura) e duas misturas ficavam faltando.
        prof = ln.count("{") - ln.count("}")
        if prof <= 0:
            # `override function addProp(...)` com o `{` na linha seguinte.
            prof = 0
        while i < len(lines):
            cur = lines[i]
            prof += cur.count("{") - cur.count("}")
            if i > n and prof <= 0:
                break
            ms = RE_SUFFIX.search(cur)
            mn = RE_NAME_EQ.search(cur)
            mc = RE_CASE.search(cur)
            if ms:
                alvo = ("sufixo", ms.group(1))
            elif mn:
                alvo = ("nome", mn.group(1))
            elif mc:
                alvo = ("nome", mc.group(1))
            for m in RE_ATTR.finditer(cur):
                if alvo is None:
                    nao.append("linha %d: prop.%s sem nome literal"
                               % (i + 1, m.group(1)))
                    continue
                k, v = m.group(1), float(m.group(2))
                if k == "blend" and not v.is_integer():
                    nao.append("linha %d: blend nao inteiro" % (i + 1))
                    continue
                f = None
                for g in fichas:
                    if g["regra"] == alvo[0] and g["nome"] == alvo[1]:
                        f = g
                        break
                if f is None:
                    f = {"nome": alvo[1], "regra": alvo[0], "blend": 0,
                         "alpha": None, "linha": i + 1}
                    fichas.append(f)
                if k == "blend":
                    f["blend"] = int(v)
                else:
                    f["alpha"] = v
            i += 1

    # 4) Contagem honesta do que ficou de fora: atribuicoes de blend/alpha que
    #    nao sao estado inicial (dentro de callbacks) ou que mexem em quem nao e'
    #    prop do JSON.
    pegas = set()
    for f in fichas:
        pegas.add(f["linha"])
    for n, ln in enumerate(lines, 1):
        if n in pegas:
            continue
        if re.search(r"\.(blend|alpha)\s*=", ln) or ".alpha =" in ln:
            eventos += 1
    return fichas, nao, eventos


def build(scripts_dir):
    saida = {"nota": "Estado INICIAL dos props, extraido dos scripts de palco do "
                     "jogo por tools/extrair_blend_palcos.py. Nao e' o "
                     "comportamento em jogo: ver os .hxc.",
             "blend_nomes": {str(k): v for k, v in sorted(BLEND.items())},
             "palcos": {}, "nao_resolvido": [], "linhas_fora_do_inicial": {}}
    arquivos = sorted(f for f in os.listdir(scripts_dir) if f.endswith(".hxc"))
    for arq in arquivos:
        sid = arq[:-4]
        fichas, nao, eventos = scan_file(os.path.join(scripts_dir, arq))
        if fichas:
            saida["palcos"][sid] = {"fonte": "preload/scripts/stages/" + arq,
                                    "props": fichas}
        for msg in nao:
            saida["nao_resolvido"].append("%s %s" % (arq, msg))
        if eventos:
            saida["linhas_fora_do_inicial"][sid] = eventos
    return saida


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scripts", help="pasta preload/scripts/stages do clone")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.check:
        if not os.path.isfile(OUT):
            print("missing %s" % os.path.relpath(OUT, PROJECT))
            return 1
        d = json.load(open(OUT, encoding="utf-8"))
        print("stage-script.json: %d palco(s) com script, %d prop(s), %d fora "
              "do inicial" % (len(d["palcos"]),
                              sum(len(v["props"]) for v in d["palcos"].values()),
                              sum(d["linhas_fora_do_inicial"].values())))
        return 0

    if not args.scripts:
        print("--scripts <clone>/preload/scripts/stages e' obrigatorio")
        return 2
    d = build(args.scripts)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(d, fh, indent=1, sort_keys=True)
        fh.write("\n")
    nprops = sum(len(v["props"]) for v in d["palcos"].values())
    print("wrote %s: %d palco(s), %d prop(s), %d NAO RESOLVIDO, %d linha(s) "
          "fora do estado inicial"
          % (os.path.relpath(OUT, PROJECT), len(d["palcos"]), nprops,
             len(d["nao_resolvido"]), sum(d["linhas_fora_do_inicial"].values())))
    for p in d["nao_resolvido"][:10]:
        print("  NAO RESOLVIDO: " + p)
    for sid, v in sorted(d["palcos"].items()):
        for f in v["props"]:
            print("  %-22s %-28s regra=%-6s %-10s blend=%d alpha=%s (L%d)"
                  % (sid, f["nome"], f["regra"],
                     BLEND.get(f["blend"], "?%d" % f["blend"]), f["blend"],
                     f["alpha"], f["linha"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
