#!/usr/bin/env python3
"""Escreve no plano do Resultado a MUSICA da tela de resultado.

O QUE O ORIGINAL FAZ (`ResultState.hx` L502-546, v0.8.6)

    new FlxTimer().start(rank.getMusicDelay(), _ -> {
      var musicPath = getMusicPath(playerCharacter, rank);          // L547-550
      var introMusic = Paths.music('$musicPath/$musicPath-intro');  // L505
      if (Assets.exists(introMusic)) { ... toca o intro, e no `onComplete`
        toca o arquivo principal em loop ... }                      // L506-524
      else { FunkinSound.playMusic(musicPath, ...) }                // L536-541
    });

`getMusicPath` = `playerCharacter?.getResultsMusicPath(rank) ?? 'resultsNORMAL'`
(L547-550) e `getResultsMusicPath` (PlayableCharacter.hx L126-145) le
`_data.results.music.<RANK>` com os padroes:

    PERFECT_GOLD, PERFECT -> "resultsPERFECT"
    EXCELLENT             -> "resultsEXCELLENT"
    GREAT, GOOD           -> "resultsNORMAL"
    SHIT                  -> "resultsSHIT"
    default               -> "resultsNORMAL"

`getMusicDelay()` (Scoring.hx L530-547), em segundos: 95/24 no PERFECT(_GOLD),
0 no EXCELLENT, 5/24 no GREAT, 3/24 no GOOD, 2/24 no SHIT.

QUEM E' O "DONO" (`PlayerRegistry.getCharacterOwnerId`, e o `?? 'bf'` de
ResultState L205-206)

O `player` de cada uma das 27 musicas deste projeto vem do
`<id>-metadata.json` -- o mesmo dado que gerou o song_registry. O dono e' o
`players/<id>.json` que lista aquele personagem em `ownedChars`; quem nao esta'
em nenhuma lista cai no `?? 'bf'` do original (e' o caso do `spaghetti`, que
toca o `sserafim-sakura`).

Cada dono tem os SEUS arquivos: `bf` -> `results*`, `pico` -> `results*-pico`
(medido no clone de arte: os dois conjuntos existem e sao diferentes, com
duracoes diferentes).

O QUE ESTE SCRIPT NAO FAZ

Nao converte audio -- quem faz isso e' `tools/empacotar_audio.py` (que le o OGG
e escreve PCM s16le 48 kHz). Aqui so' se confere que o PCM de cada caminho
existe no pacote e se escreve o bloco `musica` do `resultado-plan.json`.

Uso:
    tools/empacotar_resultado_musica.py --assets /tmp/fassets \
        [--plan assets-data/resultado-plan.json] [--packs build/packs/audio]
"""
import argparse
import glob
import json
import os
import sys

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Ordem dos ranks no port (`g_res_rank` em src/engine/resultado_dados.h:
# 0 LOSS, 1 GOOD, 2 GREAT, 3 EXCELLENT, 4 PERFECT, 5 PERFECT_GOLD) com o nome do
# rank no original. `LOSS` e' o `SHIT` do Scoring.
RANKS = ["SHIT", "GOOD", "GREAT", "EXCELLENT", "PERFECT", "PERFECT_GOLD"]

# Padroes de `getResultsMusicPath` (PlayableCharacter.hx L126-145).
PADRAO = {
    "PERFECT_GOLD": "resultsPERFECT",
    "PERFECT": "resultsPERFECT",
    "EXCELLENT": "resultsEXCELLENT",
    "GREAT": "resultsNORMAL",
    "GOOD": "resultsNORMAL",
    "SHIT": "resultsSHIT",
}

# `getMusicDelay()` em /24 de segundo (Scoring.hx L530-547). 0 no EXCELLENT.
ATRASO_Q = [2, 3, 5, 0, 95, 95]

# Nome do PCM de cada arquivo de musica: o mesmo de `RESULTADOS` em
# `tools/empacotar_audio.py`. O `-intro` existe em algumas pastas e nao em
# outras (medido no clone); o nome do pacote troca `-` por `_`.
def nome_pcm(base, intro, sufixo):
    nome = base.replace("results", "res_").replace("-", "_").lower()
    if sufixo:
        nome += "_" + sufixo.replace("-", "_")
    if intro:
        nome += "_intro"
    return nome


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True, help="clone do funkin.assets")
    ap.add_argument("--plan", default=os.path.join(PROJECT, "assets-data",
                                                  "resultado-plan.json"))
    ap.add_argument("--packs", default=os.path.join(PROJECT, "build", "packs",
                                                    "audio"))
    ap.add_argument("--songs", default=os.path.join(PROJECT, "assets-data",
                                                    "songs"))
    args = ap.parse_args()

    # 1) os donos e o que cada um possui
    donos = []
    for caminho in sorted(glob.glob(os.path.join(args.assets, "preload/data/players/*.json"))):
        d = json.load(open(caminho, encoding="utf-8"))
        ident = os.path.splitext(os.path.basename(caminho))[0]
        musica = (d.get("results") or {}).get("music") or {}
        donos.append({"id": ident,
                      "owned": list(d.get("ownedChars") or []),
                      "musica": musica})
    if not donos:
        print("ERRO: nenhum players/*.json no clone de arte", file=sys.stderr)
        return 1

    # 2) o `player` de cada musica do projeto + o dono (o `?? 'bf'` do original)
    jogadores = set()
    for caminho in sorted(glob.glob(os.path.join(args.songs, "*-metadata.json"))):
        d = json.load(open(caminho, encoding="utf-8"))
        p = ((d.get("playData") or {}).get("characters") or {}).get("player")
        if p:
            jogadores.add(p)
    if not jogadores:
        print("ERRO: nenhum <id>-metadata.json em %s" % args.songs, file=sys.stderr)
        return 1
    padrao_dono = "bf"
    if padrao_dono not in [x["id"] for x in donos]:
        print("ERRO: o dono padrao 'bf' nao esta' em players/", file=sys.stderr)
        return 1
    tabela = []
    for p in sorted(jogadores):
        achado = None
        for dono in donos:
            if p in dono["owned"]:
                achado = dono["id"]
                break
        tabela.append([p, achado if achado else padrao_dono])

    # 3) a musica de cada dono em cada rank: o `results.music` do JSON, senao o
    #    padrao do `getResultsMusicPath`, senao "<padrao>-<dono>" (o sufixo que
    #    separa os conjuntos de arte do bf e do pico).
    blocos = {}
    faltando = []
    for dono in donos:
        linhas = []
        for rank in RANKS:
            base = (dono["musica"].get(rank) or PADRAO[rank])
            if dono["id"] != padrao_dono and not base.endswith("-" + dono["id"]):
                base = base + "-" + dono["id"]
            intro = nome_pcm(base, True, None)
            principal = nome_pcm(base, False, None)
            tem_intro = os.path.isfile(os.path.join(args.packs, intro + ".pcm"))
            if not os.path.isfile(os.path.join(args.packs, principal + ".pcm")):
                faltando.append(principal)
            linhas.append({"rank": rank, "arquivo": principal,
                           "intro": intro if tem_intro else "",
                           "base": base})
        blocos[dono["id"]] = linhas

    # 4) o bloco `musica` do plano
    plano = json.load(open(args.plan, encoding="utf-8"))
    plano["musica"] = {
        "ranks": RANKS,
        "atraso_q": ATRASO_Q,
        "donos": [d["id"] for d in donos],
        "padrao": padrao_dono,
        "jogadores": tabela,
        "tabela": {dono: blocos[dono] for dono in blocos},
        "linha": "ResultState.hx L502-550; Scoring.hx L530-547; "
                 "PlayableCharacter.hx L126-145",
    }
    with open(args.plan, "w", encoding="utf-8") as f:
        json.dump(plano, f, ensure_ascii=False, indent=1)
        f.write("\n")

    for dono, linhas in blocos.items():
        print("  %-6s %s" % (dono, "; ".join(
            "%s%s" % (l["arquivo"], (" + intro " + l["intro"]) if l["intro"] else "")
            for l in linhas)))
    print("  jogadores: %s" % ", ".join("%s->%s" % (p, o) for p, o in tabela))
    if faltando:
        print("  FALTANDO no pacote de audio: " + ", ".join(sorted(set(faltando))),
              file=sys.stderr)
        print("  (rode tools/empacotar_audio.py --assets <clone>)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
