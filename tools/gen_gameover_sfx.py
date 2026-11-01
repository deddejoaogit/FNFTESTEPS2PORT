#!/usr/bin/env python3
"""Gera src/gameover_dados.h: os sufixos de game over de cada personagem.

POR QUE ESTE ARQUIVO EXISTE

`GameOverSubState.musicSuffix` / `blueBallSuffix` / `animationSuffix` (e o
equivalente da pausa) NAO estao no CharacterData: cada personagem os define no
seu script (`preload/scripts/characters/<id>.hxc`, funcao `onCreate`). O pacote
de dados deste repositorio (`assets-data/`) nao traz scripts, entao o port nao
tinha como saber que o `bf-pixel` toca `gameOver-pixel` e o `pico-blazin` toca
`gameOver-pico` + `fnf_loss_sfx-pico-gutpunch`.

Isso e' DADO AUSENTE no pacote, nao constante desconhecida: o valor esta' escrito
no script do jogo original. Este gerador le' os scripts do clone de arte
(`funkin.assets.preload/scripts`) e transcreve o que esta' la', sem inventar: o
que nao aparece no script fica vazio, que e' exatamente o padrao do original
(`GameOverSubState.reset()` zera os tres campos e `PlayState.reset()` chama
`reset`).

Tambem transcreve a PAUSA (`PauseSubState.musicSuffix`) porque e' o mesmo
mecanismo e o mesmo lugar; o port ainda nao toca a musica de pausa, mas quando
tocar o valor ja' vai estar aqui em vez de ser procurado de novo.

O QUE ESTE GERADOR NAO PEGA (e por isso fica registrado em vez de chutado)

`songs/2hot.hxc` muda os sufixos para `-pico-explode` em tempo de execucao, e so'
quando o jogador morre com a lata explodindo (`takeCanDamage`, L436-448). Isso e'
logica de script de musica, nao configuracao de personagem: nao ha' linha em
lugar nenhum dizendo "a musica 2hot usa -pico-explode". O port mantem o sufixo
do personagem (`-pico`) e a condicao continua ausente -- sem chute.

Uso:
    PYTHONPATH=/tmp/pylibs tools/gen_gameover_sfx.py --assets /tmp/fassets [--check]
"""
import argparse
import os
import re
import sys

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DESTINO = os.path.join(PROJECT, "src", "gameover_dados.h")

CAMPOS = [
    ("musicSuffix", "musica"),
    ("blueBallSuffix", "blueball"),
    ("animationSuffix", "anim"),
    ("mustNotExit", None),
]


def le_scripts(base):
    """{id: {'musica': '-pico', ...}} a partir de preload/scripts/characters."""
    pasta = os.path.join(base, "preload", "scripts", "characters")
    if not os.path.isdir(pasta):
        return None
    out = {}
    for nome in sorted(os.listdir(pasta)):
        if not nome.endswith(".hxc"):
            continue
        cid = nome[:-4]
        texto = open(os.path.join(pasta, nome), encoding="utf-8", errors="replace").read()
        achados = {}
        for campo, chave in CAMPOS:
            if chave is None:
                continue
            # So' atribuicoes com literal: `GameOverSubState.musicSuffix = '-pico';`
            m = re.search(r"GameOverSubState\.%s\s*=\s*'([^']*)'\s*;" % campo, texto)
            if m:
                achados[chave] = m.group(1)
        pausa = re.search(r"PauseSubState\.musicSuffix\s*=\s*'([^']*)'\s*;", texto)
        if pausa:
            achados["pausa"] = pausa.group(1)
        if achados:
            out[cid] = achados
    return out


def esc(v):
    return '"' + v + '"'


def monta(dados, versao):
    linhas = [
        "/* GERADO POR tools/gen_gameover_sfx.py - NAO EDITAR A MAO.",
        "   Fonte: preload/scripts/characters/*.hxc do funkin.assets (%s)," % versao,
        "   funcao onCreate de cada personagem. Somente atribuicoes literais;",
        "   personagem que nao atribui fica fora da tabela e o jogo usa \"\",",
        "   que e' o que GameOverSubState.reset() faz (PlayState.reset chama). */",
        "#ifndef GAMEOVER_DADOS_H",
        "#define GAMEOVER_DADOS_H",
        "",
        "typedef struct{",
        "  const char*id;",
        "  const char*musica;   /* GameOverSubState.musicSuffix   */",
        "  const char*blueball; /* GameOverSubState.blueBallSuffix */",
        "  const char*anim;     /* GameOverSubState.animationSuffix */",
        "  const char*pausa;    /* PauseSubState.musicSuffix      */",
        "}go_sfx_dados_t;",
        "",
        "static const go_sfx_dados_t go_sfx_chars[]={",
    ]
    for cid in sorted(dados):
        d = dados[cid]
        linhas.append('  {%s,%s,%s,%s,%s},' % (
            esc(cid),
            esc(d.get("musica", "")),
            esc(d.get("blueball", "")),
            esc(d.get("anim", "")),
            esc(d.get("pausa", "")),
        ))
    linhas += [
        "};",
        "#define GO_SFX_CHARS_COUNT ((int)(sizeof(go_sfx_chars)/sizeof(go_sfx_chars[0])))",
        "",
        "#endif /* GAMEOVER_DADOS_H */",
        "",
    ]
    return "\n".join(linhas)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", default=os.environ.get("FNF_ASSETS", ""))
    ap.add_argument("--check", action="store_true",
                    help="nao escreve: compara com o arquivo commitado")
    args = ap.parse_args()
    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets> (ou FNF_ASSETS)")
        return 2
    dados = le_scripts(args.assets)
    if dados is None:
        print("  FALTA: %s/preload/scripts/characters" % args.assets)
        return 2
    versao = "clone local"
    rev = os.path.join(args.assets, ".git")
    if os.path.isdir(rev):
        try:
            import subprocess
            versao = subprocess.check_output(
                ["git", "-C", args.assets, "rev-parse", "--short", "HEAD"]).decode().strip()
        except Exception:
            pass
    texto = monta(dados, versao)
    if args.check:
        if not os.path.isfile(DESTINO):
            print("  FALTA: %s" % DESTINO)
            return 1
        atual = open(DESTINO, encoding="utf-8").read()
        if atual != texto:
            print("  DIFERE: %s nao corresponde aos scripts" % DESTINO)
            return 1
        print("  ok:   gameover_dados.h confere com %d script(s)" % len(dados))
        return 0
    with open(DESTINO, "w", encoding="utf-8") as f:
        f.write(texto)
    print("  %d personagem(ns) com sufixo de game over/pausa em %s"
          % (len(dados), os.path.relpath(DESTINO, PROJECT)))
    for cid in sorted(dados):
        d = dados[cid]
        print("    %-20s musica=%-8s blueball=%-16s anim=%-11s pausa=%s"
              % (cid, d.get("musica", ""), d.get("blueball", ""),
                 d.get("anim", ""), d.get("pausa", "")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
