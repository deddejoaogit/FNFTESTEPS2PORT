#!/usr/bin/env python3
"""Inventario dos scripts de palco (`.hxc`) contra o que o port desenha.

Por que existe: a tabela de pendencias do `docs/PLANO_COMPLETO.md` diz que o
port usa dos scripts so' o **estado inicial** de blend/alpha dos props do JSON,
e conta "77 linhas". Esse numero nunca disse *quais* linhas, *em que palco* e
*se o prop existe no JSON* -- entao nao dava para decidir o que portar primeiro.

O que ele faz, sem chutar:

  1. le cada `preload/scripts/stages/*.hxc` do clone da arte;
  2. acha a funcao que contem cada linha (para separar `buildStage`/`onCreate`,
     que e' o estado que o port JA tem, do resto, que roda durante a musica);
  3. classifica a linha pelo que ela muda: `alpha`, `visible`, `blend`,
     `position`, `animation`, `color` (tinta de personagem), `shader`,
     `sound`/`haptic`, `create` (sprite criado por script);
  4. diz se o prop citado existe no `assets-data/stages/<palco>.json` do port
     -- `getNamedProp('x')` num prop que nao esta' no JSON e' desenho que o
     port nao tem como fazer sem dado novo.

Uso:
    python3 tools/analisa_scripts_palco.py --assets /tmp/fassets [--json]

O `--json` imprime o inventario em JSON (para outra ferramenta consumir).

NAO e' um interpretador de HScript e nao tenta ser: ele **conta e localiza**,
que e' o que a regra 4 do `REGRAS.md` pede antes de qualquer codigo (sem dado
nao se adivinha; primeiro se mede).
"""
import argparse
import glob
import json
import os
import re
import sys

# Cada categoria com o que ela exige do port hoje. A ordem e' a do custo de
# portar: o que esta' no topo e' dado que o port ja' desenha.
CATEGORIAS = [
    ("alpha", r"\.alpha\s*=", "prop que o port desenha (basta animar o alfa)"),
    ("visible", r"\.visible\s*=", "prop que o port desenha (basta ligar/desligar)"),
    ("blend", r"\.blend\s*=", "GS: so' NORMAL e ADD na pipeline fixa"),
    ("position", r"\.(x|y)\s*=", "posicao no espaco do palco"),
    ("animation", r"\.animation\.play\(", "animacao do prop (se o .cfz4 tem a animacao)"),
    ("color", r"\.color\s*=|FlxTween\.color", "tinta por personagem -- o port nao tinge"),
    ("shader", r"[Ss]hader", "filtro de GPU -- o GS nao tem"),
    ("sound", r"FunkinSound\.play|soundRandom|Paths\.sound", "som do palco"),
    ("haptic", r"HapticUtil\.vibrate", "vibracao -- nao existe no console"),
    ("create", r"new (FlxTiledSprite|FlxBackdrop|FunkinSprite|FlxSprite|FlxText)",
     "sprite criado por script (nao esta' no JSON)"),
    ("timer", r"new FlxTimer|FlxTween\.tween", "agendamento (tween/timer)"),
    ("char_visible", r"get(Boyfriend|Dad|Girlfriend|Girlfriend)\(\)\??\.visible|"
                     r"currentStage\.get(Dad|Boyfriend|Girlfriend)\(\)\??\.visible",
     "quem aparece no palco"),
]

# Estado inicial: essas funcoes rodam ANTES da musica e o port ja' tem o
# resultado delas no `stage_dados.h` (via `tools/extrair_blend_palcos.py`).
FUNCOES_INICIAIS = ("buildStage", "onCreate")

FUNCAO_RE = re.compile(r"^\s*(?:override\s+)?(?:public\s+|private\s+)?function\s+(\w+)\s*\(")
PROP_RE = re.compile(r"getNamedProp\(\s*'([^']+)'\s*\)")


def funcoes_por_linha(texto):
    """Mapa linha -> nome da funcao que a contem (0 = topo da classe)."""
    mapa = {}
    atual = "(topo)"
    for i, linha in enumerate(texto.splitlines(), start=1):
        m = FUNCAO_RE.match(linha)
        if m:
            atual = m.group(1)
        mapa[i] = atual
    return mapa


def props_do_json(repo, palco):
    caminho = os.path.join(repo, "assets-data", "stages", palco + ".json")
    if not os.path.isfile(caminho):
        return None
    with open(caminho, encoding="utf-8") as f:
        d = json.load(f)
    return {p.get("name") for p in d.get("props", [])}


def classifica(linha):
    achados = []
    for nome, rx, _ in CATEGORIAS:
        if re.search(rx, linha):
            achados.append(nome)
    return achados


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True, help="clone do funkin.assets")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    pasta = os.path.join(args.assets, "preload", "scripts", "stages")
    arquivos = sorted(glob.glob(os.path.join(pasta, "*.hxc")))
    if not arquivos:
        sys.exit("nao achei nenhum .hxc em %s" % pasta)

    inventario = {}
    for caminho in arquivos:
        palco = os.path.basename(caminho)[:-4]
        with open(caminho, encoding="utf-8") as f:
            texto = f.read()
        mapa = funcoes_por_linha(texto)
        do_json = props_do_json(args.repo, palco)
        linhas = []
        for i, linha in enumerate(texto.splitlines(), start=1):
            limpa = linha.strip()
            if not limpa or limpa.startswith("//"):
                continue
            cats = classifica(limpa)
            if not cats:
                continue
            dono = mapa.get(i, "?")
            props = PROP_RE.findall(limpa)
            # `getNamedProp` numa variavel (`var p = getNamedProp('x')`) e'
            # guardado para as linhas seguintes do mesmo escopo: o inventario
            # so' promete o que a LINHA diz, nao o fluxo inteiro.
            linhas.append({
                "linha": i,
                "funcao": dono,
                "inicial": dono in FUNCOES_INICIAIS,
                "cats": cats,
                "props": props,
                "props_no_json": (None if do_json is None
                                  else [p for p in props if p in do_json]),
                "texto": limpa,
            })
        inventario[palco] = {"json": do_json is not None, "linhas": linhas}

    if args.json:
        json.dump(inventario, sys.stdout, ensure_ascii=False, indent=1)
        print()
        return

    total = sum(len(v["linhas"]) for v in inventario.values())
    fora = [l for v in inventario.values() for l in v["linhas"] if not l["inicial"]]
    print("== scripts de palco: %d arquivos, %d linhas que mudam estado" %
          (len(inventario), total))
    print("   fora do estado inicial (rodam durante a musica): %d" % len(fora))
    print()
    print("%-26s %5s %5s %6s %6s  %s" %
          ("palco", "linhas", "fora", "com", "s/", "categorias fora do inicial"))
    print("%-26s %5s %5s %6s %6s  %s" %
          ("", "", "", "json", "json", ""))
    for palco in sorted(inventario):
        v = inventario[palco]
        f = [l for l in v["linhas"] if not l["inicial"]]
        cats = {}
        for l in f:
            for c in l["cats"]:
                cats[c] = cats.get(c, 0) + 1
        com_json = sum(1 for l in f if l["props"] and l["props_no_json"]
                       and len(l["props_no_json"]) == len(l["props"]))
        sem_json = sum(1 for l in f if l["props"] and l["props_no_json"] is not None
                       and len(l["props_no_json"]) < len(l["props"]))
        desc = " ".join("%s=%d" % (k, cats[k]) for k in sorted(cats))
        print("%-26s %5d %5d %6d %6d  %s" %
              (palco, len(v["linhas"]), len(f), com_json, sem_json, desc))
    print()
    print("== por categoria, so' o que roda durante a musica")
    for nome, _, nota in CATEGORIAS:
        n = sum(1 for l in fora if nome in l["cats"])
        print("  %-13s %4d   (%s)" % (nome, n, nota))
    print()
    faltando = {}
    for palco, v in inventario.items():
        for l in v["linhas"]:
            for p in l["props"]:
                if l["props_no_json"] is not None and p not in l["props_no_json"]:
                    faltando.setdefault(palco, set()).add(p)
    print("== props citados por `getNamedProp` que NAO existem no JSON do port")
    if not faltando:
        print("  (nenhum)")
    for palco in sorted(faltando):
        print("  %-26s %s" % (palco, ", ".join(sorted(faltando[palco]))))


if __name__ == "__main__":
    main()
