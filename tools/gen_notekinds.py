#!/usr/bin/env python3
"""Le' os NOTE KINDS dos scripts do original e escreve o dado do port.

POR QUE EXISTE
O port reportava 32 dos 34 kinds dos charts como "script nao esta' no pacote de
dados". Estava errado: os scripts existem, em `<clone>/preload/scripts/`, e este
programa os le'. Ler em vez de pedir ao usuario e' a regra 4 (dado que falta se
COLETA; nao se adivinha).

O QUE ELE LE', E DE ONDE
  1. `preload/scripts/notekinds/*.hxc`: cada kind e' uma classe que faz
     `super('id', 'Nome de exibicao')`. E' a lista FECHADA dos kinds de script
     (32) -- os outros dois (`noanim`, `non_scoreable`) sao codigo do jogo.
  2. `preload/scripts/characters/*.hxc`: o comportamento visivel de cada kind e'
     um `switch (event.note.kind)` dentro de `onNoteHit`, com dois formatos:
       `this.playSingAnimation(dir, false, 'SUFIXO')`  -> canta "sing<DIR>-SUFIXO"
       `playXxxAnim()`                                 -> toca a animacao que a
                                                          funcao homonima pede
                                                          (`playAnimation('nome')`)
     Junto das duas, o script pode pedir `moveToBack()`/`moveToFront()` e
     `PlayState.instance?.camGame?.shake(forca, segundos)`. Tudo isso vai para o
     dado, porque e' evento de jogo: o port tem de saber que existe.

O QUE ELE NAO TENTA
Condicoes (`cantUppercut`, `wasNoteHitPoorly`, sorteio de 30%) mudam QUAL
animacao toca. Elas ficam registradas como `condicional` no dado -- o port sabe
que o kind tem mais de um caminho, e nao escolhe um no chute.

USO
    python3 tools/gen_notekinds.py --assets /tmp/fassets
        -> assets-data/notekinds.json       (dado, para conferencia e diff)
        -> src/engine/notekind_dados.h      (o mesmo dado em C, para o jogo)
"""

import argparse
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)


# ------------------------------------------------------------------ leitura .hxc
def sem_comentarios(txt):
    txt = re.sub(r"/\*.*?\*/", "", txt, flags=re.S)
    return re.sub(r"//[^\n]*", "", txt)


def registro_dos_kinds(texto):
    """[(id, nome_de_exibicao)] de um arquivo de notekinds/."""
    out = []
    # as tres formas que o original usa: 'id' + "nome", "id" + "nome" e
    # 'id' + 'nome' (ugh e hehPrettyGood sao assim). A terceira faltava, e dois
    # kinds sumiam da lista sem nenhum aviso.
    padroes = [
        r'super\(\s*\'([^\']+)\'\s*,\s*"([^"]*)"\s*\)',
        r'super\(\s*"([^"]+)"\s*,\s*"([^"]*)"\s*\)',
        r"super\(\s*'([^']+)'\s*,\s*'([^']*)'\s*\)",
    ]
    for pat in padroes:
        for m in re.finditer(pat, texto):
            out.append((m.group(1), m.group(2)))
    vistos, uniq = set(), []
    for k, n in out:
        if k not in vistos:
            vistos.add(k)
            uniq.append((k, n))
    return uniq


def corpo_da_funcao(texto, nome):
    """O texto entre as chaves de `function <nome>(...)` (fecha no nivel 0)."""
    m = re.search(r"function\s+" + re.escape(nome) + r"\s*\(", texto)
    if not m:
        return None
    i = texto.find("{", m.end())
    if i < 0:
        return None
    prof, j = 0, i
    while j < len(texto):
        if texto[j] == "{":
            prof += 1
        elif texto[j] == "}":
            prof -= 1
            if prof == 0:
                return texto[i:j + 1]
        j += 1
    return texto[i:]


ANIM = re.compile(r"playAnimation\(\s*'([^']+)'")
# `this.playAnimation('punchHigh' + postfix, ...)`: o nome nao esta' escrito
# inteiro no script. O `postfix` vem de uma funcao que alterna dois literais
# (`alternate = !alternate; return alternate ? '1' : '2';`), e os dois nomes
# resultantes ESTAO nos dados do personagem ('punchHigh1'/'punchHigh2'). Ler os
# dois e' o que evita 190 dos 203 "animacao que o script pede e a arte nao tem".
ANIM_VAR = re.compile(r"playAnimation\(\s*'([^']+)'\s*\+\s*([A-Za-z_][A-Za-z0-9_]*)")
ALTERNA = re.compile(r"\?\s*'([^']*)'\s*:\s*'([^']*)'")


def funcao_de_anim(texto, nome):
    """`playFooAnim()` -> {'anim':..., 'ordem':..., 'shake':..., 'alterna':...}"""
    corpo = corpo_da_funcao(texto, nome)
    if corpo is None:
        return None
    r = {}
    # Nome MONTADO primeiro: `'punchHigh' + postfix` tambem casa com o padrao
    # simples (que veria so' 'punchHigh'), e essa leitura era a que produzia
    # 93 "animacao que o script pede e a arte nao tem" numa unica passada.
    v = ANIM_VAR.search(corpo)
    if v:
        base, var = v.group(1), v.group(2)
        # `var postfix:String = doAlternate();` -- o sufixo vem da FUNCAO, nao
        # da variavel: procurar uma funcao chamada `postfix` nao acha nada.
        m = re.search(r"(?:var\s+)?" + re.escape(var) +
                      r"\s*(?::\s*\w+)?\s*=\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(", corpo)
        fonte = m.group(1) if m else var
        corpo_var = corpo_da_funcao(texto, fonte)
        alt = ALTERNA.search(corpo_var or "")
        if alt:
            r["anim"] = base + alt.group(1)
            r["anim2"] = base + alt.group(2)
            r["alterna"] = 1
    if not r:
        m = ANIM.search(corpo)
        if m:
            r["anim"] = m.group(1)
    if "moveToBack()" in corpo:
        r["ordem"] = "fundo"
    elif "moveToFront()" in corpo:
        r["ordem"] = "frente"
    m = re.search(r"shake\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*\)", corpo)
    if m:
        r["shake"] = [float(m.group(1)), float(m.group(2))]
    return r or None


CASE = re.compile(r"case\s*\"([^\"]+)\"\s*:")
CHAMADA = re.compile(r"(playSingAnimation|play[A-Za-z]+Anim)\s*\(")
SUFIXO = re.compile(r"playSingAnimation\([^;]*?,\s*(?:false|true)\s*,\s*'([^']+)'\s*\)")


def comportamento_dos_kinds(texto):
    """{kind: {...}} lido do `onNoteHit` do personagem."""
    corpo = corpo_da_funcao(texto, "onNoteHit")
    if corpo is None:
        return {}
    out = {}
    marcas = [(m.start(), m.group(1)) for m in CASE.finditer(corpo)]
    for i, (pos, kind) in enumerate(marcas):
        fim = marcas[i + 1][0] if i + 1 < len(marcas) else len(corpo)
        trecho = corpo[pos:fim]
        # corta no `return`/`break` que fecha o case
        corte = re.search(r"\b(return|break)\s*;", trecho)
        if corte:
            trecho = trecho[:corte.start()]
        b = {}
        m = SUFIXO.search(trecho)
        if m:
            b["tipo"] = "sing"
            b["sufixo"] = m.group(1)
        else:
            m = CHAMADA.search(trecho)
            if m and m.group(1) != "playSingAnimation":
                r = funcao_de_anim(texto, m.group(1))
                if r:
                    b["tipo"] = "anim"
                    b.update(r)
            else:
                # o case pode chamar a animacao direto, sem funcao auxiliar
                # (`this.playAnimation('ugh', true, true)` no tankman.hxc).
                d = ANIM.search(trecho)
                if d:
                    b["tipo"] = "anim"
                    b["anim"] = d.group(1)
                    if "moveToBack()" in trecho:
                        b["ordem"] = "fundo"
                    elif "moveToFront()" in trecho:
                        b["ordem"] = "frente"
        # condicao: a mesma funcao aparece mais de uma vez no case, ou o case
        # depende de estado (`if (...)` antes da chamada)
        if trecho.count("if (") > 0 or trecho.count("if(") > 0:
            b["condicional"] = 1
        if b:
            out[kind] = b
    return out


CODE_KINDS = ("noanim", "non_scoreable")


def kinds_dos_charts(projeto):
    """{kind: n_notas} lido dos 81 charts -- a fonte de 'quais kinds o jogo ve'."""
    out = {}
    d = os.path.join(projeto, "assets-data", "charts")
    for p in sorted(glob.glob(os.path.join(d, "*.json"))):
        with open(p, encoding="utf-8-sig") as f:
            doc = json.load(f)
        for _diff, notas in (doc.get("notes") or {}).items():
            for n in notas:
                k = n.get("k")
                if k:
                    out[k] = out.get(k, 0) + 1
    return out


def conferir(projeto):
    """O dado commitado cobre os kinds que os charts usam?

    Sem esta conferencia, um kind usado numa musica e ausente da tabela passaria
    calado -- e o jogo o trataria como "nao ha nada a fazer", que e' exatamente o
    bug que este dado existe para consertar (34 kinds, 32 sem tratamento).
    """
    q = os.path.join(projeto, "assets-data", "notekinds.json")
    if not os.path.isfile(q):
        return ["falta assets-data/notekinds.json"]
    with open(q, encoding="utf-8") as f:
        dado = json.load(f)
    reg = set(dado["kinds"])
    usados = kinds_dos_charts(projeto)
    # Um kind pode existir SEM classe NoteKind registrada: `sakura-joint`,
    # `sakura-bf1` e `sakura-bf2` sao strings que o script do personagem
    # compara e nada mais -- nao ha' `super('sakura-joint', ...)` em lugar
    # nenhum. Para esses nao existe nome de exibicao, e inventar um seria
    # divergencia; o que o jogo precisa (o que TOCAR) esta' na acao do
    # personagem. Por isso a conferencia separa os dois casos:
    #   - sem registro E sem acao  -> FALHA (o kind nao faz nada em lugar nenhum)
    #   - sem registro, com acao   -> aceito, e reportado aqui
    com_acao = set()
    for _c, acoes in dado["personagens"].items():
        com_acao.update(acoes)
    orfaos = sorted(k for k in usados
                    if k not in reg and k not in CODE_KINDS and k not in com_acao)
    sem_classe = sorted(k for k in usados if k not in reg and k in com_acao)
    erros = []
    if orfaos:
        erros.append("kinds usados nos charts sem registro e sem acao: %r" % orfaos)
    if sem_classe:
        print("notekinds: %d kinds sem classe NoteKind, resolvidos so' pela acao do "
              "personagem: %s" % (len(sem_classe), ", ".join(sem_classe)))
    # todo kind do registro tem nome de exibicao? (nomes vazios = leitura errada)
    vazios = sorted(k for k, v in dado["kinds"].items() if not v.get("nome"))
    if erros or vazios:
        erros.append("kinds sem nome de exibicao: %r" % vazios)
    # toda acao tem tipo conhecido e valor nao vazio
    ruins = []
    for c, acoes in dado["personagens"].items():
        for k, b in acoes.items():
            if b.get("tipo") not in ("sing", "anim"):
                ruins.append((c, k, "tipo"))
            elif not (b.get("sufixo") if b["tipo"] == "sing" else b.get("anim")):
                ruins.append((c, k, "valor"))
    if ruins:
        erros.append("acoes sem tipo/valor: %r" % ruins[:5])
    print("notekinds: %d kinds lidos, %d usados nos 81 charts, %d de codigo (%s)"
          % (len(reg), len(usados), len(CODE_KINDS), ", ".join(CODE_KINDS)))
    print("notekinds: %d personagens com comportamento, %d associacoes"
          % (len(dado["personagens"]),
             sum(len(v) for v in dado["personagens"].values())))
    if erros:
        for e in erros:
            print("  FALHA: %s" % e)
        return 1
    print("notekinds: o dado cobre TODOS os kinds que aparecem nos charts")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="confere o dado commitado contra os charts (sem clone)")
    ap.add_argument("--assets",
                    help="clone de funkin.assets (com preload/scripts)")
    ap.add_argument("--json", default=os.path.join(PROJECT, "assets-data", "notekinds.json"))
    ap.add_argument("--header", default=os.path.join(PROJECT, "src", "engine",
                                                     "notekind_dados.h"))
    args = ap.parse_args()

    if args.check:
        return conferir(PROJECT)
    if not args.assets:
        raise SystemExit("precisa de --assets (ou --check)")

    base = os.path.join(args.assets, "preload", "scripts")
    if not os.path.isdir(base):
        raise SystemExit("sem %s -- rode o sparse-checkout do clone da arte" % base)

    kinds, origem = {}, {}
    for p in sorted(glob.glob(os.path.join(base, "notekinds", "*.hxc"))):
        txt = sem_comentarios(open(p, encoding="utf-8-sig").read())
        for k, n in registro_dos_kinds(txt):
            kinds[k] = n
            origem[k] = os.path.relpath(p, args.assets).replace(os.sep, "/")
    if not kinds:
        raise SystemExit("nenhum kind lido em notekinds/")

    por_char = {}
    for p in sorted(glob.glob(os.path.join(base, "characters", "*.hxc"))):
        cid = os.path.splitext(os.path.basename(p))[0]
        txt = sem_comentarios(open(p, encoding="utf-8-sig").read())
        b = comportamento_dos_kinds(txt)
        if b:
            por_char[cid] = b

    dado = {
        "fonte": "preload/scripts/notekinds/ e preload/scripts/characters/ (.hxc)",
        "kinds": {k: {"nome": kinds[k], "arquivo": origem[k]} for k in sorted(kinds)},
        "personagens": {c: {k: por_char[c][k] for k in sorted(por_char[c])}
                        for c in sorted(por_char)},
    }
    os.makedirs(os.path.dirname(args.json), exist_ok=True)
    with open(args.json, "w", encoding="utf-8") as f:
        json.dump(dado, f, indent=1, ensure_ascii=False, sort_keys=True)
        f.write("\n")

    n_assoc = sum(len(v) for v in por_char.values())
    print("kinds de script: %d" % len(kinds))
    print("personagens com comportamento de kind: %d (%d associacoes)"
          % (len(por_char), n_assoc))
    for c in sorted(por_char):
        print("  %-18s %2d kinds" % (c, len(por_char[c])))
    escrever_cabecalho(args.header, dado)
    print("escrito: %s" % os.path.relpath(args.json, PROJECT))
    print("escrito: %s" % os.path.relpath(args.header, PROJECT))
    return 0


# ------------------------------------------------------------------- gerar o .h
def escrever_cabecalho(destino, dado):
    linhas = []
    a = linhas.append
    a("/* notekind_dados - GERADO por tools/gen_notekinds.py. NAO EDITE.")
    a(" *")
    a(" * Fonte lida do clone da arte: %s" % dado["fonte"].replace("*/", "* /"))
    a(" *")
    a(" * Por que este dado existe: 34 kinds aparecem nos 81 charts e o port so'")
    a(" * implementava dois (`noanim`, `non_scoreable`). Os outros 32 estao nos")
    a(" * scripts do original, e o que o script FAZ pode ser lido: escolher a")
    a(" * animacao de canto com sufixo (`sing<DIR>-<sufixo>`) ou tocar uma")
    a(" * animacao nomeada. E' isso que esta' aqui -- nada de comportamento")
    a(" * condicional, que o dado marca com `condicional` e o jogo NAO adivinha.")
    a(" */")
    a("#ifndef FNF_NOTEKIND_DADOS_H")
    a("#define FNF_NOTEKIND_DADOS_H")
    a("")
    ks = dado["kinds"]
    a("#define NK_KIND_COUNT %du" % len(ks))
    a("")
    a("/* id -> nome de exibicao (o segundo argumento do `super` da classe). */")
    a("typedef struct {")
    a(" const char *id;")
    a(" const char *nome;")
    a("} nk_kind_t;")
    a("")
    a("static const nk_kind_t nk_kinds[NK_KIND_COUNT]={")
    for k in sorted(ks):
        a(" {\"%s\",\"%s\"}," % (k, ks[k]["nome"].replace('"', '\\"')))
    a("};")
    a("")
    a("/* O que o kind faz no personagem: 0 = nada lido, 1 = canto com sufixo,")
    a("   2 = animacao nomeada. `ordem` (0 nada, 1 fundo, 2 frente) e `shake`")
    a("   (forca x1000, ms) vem do mesmo trecho do script. */")
    a("typedef struct {")
    a(" const char *kind;")
    a(" unsigned char tipo;")
    a(" unsigned char ordem;")
    a(" unsigned char condicional;")
    a(" const char *valor;   /* sufixo do canto, ou nome da animacao */")
    a(" const char *valor2;  /* a outra metade quando o nome ALTERNA (punchHigh1/2) */")
    a(" unsigned char alterna;")
    a(" int shake_forca1000;")
    a(" int shake_ms;")
    a("} nk_acao_t;")
    a("")
    por = dado["personagens"]
    indice = []
    linhas_de = []
    for c in sorted(por):
        indice.append(c)
        acoes = []
        for k in sorted(por[c]):
            b = por[c][k]
            tipo = 1 if b.get("tipo") == "sing" else (2 if b.get("tipo") == "anim" else 0)
            ordem = {"fundo": 1, "frente": 2}.get(b.get("ordem", ""), 0)
            valor = b.get("sufixo") if tipo == 1 else b.get("anim")
            valor2 = "" if tipo == 1 else (b.get("anim2") or "")
            alt = 1 if (tipo == 2 and valor2) else 0
            sh = b.get("shake") or [0, 0]
            acoes.append((k, tipo, ordem, int(b.get("condicional", 0)),
                          valor or "", valor2, alt,
                          int(round(sh[0] * 1000)), int(round(sh[1] * 1000))))
        nome_tab = "nk_" + re.sub(r"[^a-z0-9]", "_", c)
        linhas_de.append((c, nome_tab, acoes))
    for c, tab, acoes in linhas_de:
        a("static const nk_acao_t %s[]={" % tab)
        for (k, tipo, ordem, cond, valor, valor2, alt, f, ms) in acoes:
            a(" {\"%s\",%d,%d,%d,\"%s\",\"%s\",%d,%d,%d}," % (k, tipo, ordem, cond,
                                                               valor, valor2, alt, f, ms))
        a("};")
    a("")
    a("#define NK_CHAR_COUNT %du" % len(linhas_de))
    a("typedef struct {")
    a(" const char *personagem;")
    a(" const nk_acao_t *acoes;")
    a(" unsigned int n;")
    a("} nk_char_t;")
    a("")
    a("static const nk_char_t nk_chars[NK_CHAR_COUNT]={")
    for c, tab, acoes in linhas_de:
        a(" {\"%s\",%s,%du}," % (c, tab, len(acoes)))
    a("};")
    a("")
    a("#endif /* FNF_NOTEKIND_DADOS_H */")
    with open(destino, "w", encoding="utf-8") as f:
        f.write("\n".join(linhas) + "\n")


if __name__ == "__main__":
    sys.exit(main())
