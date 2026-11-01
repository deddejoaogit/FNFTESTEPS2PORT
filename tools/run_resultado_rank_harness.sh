#!/bin/sh
# O RANK DA TELA DE RESULTADO: os seis casos, um por rodada.
#
# POR QUE EXISTE
#
# O roteiro do harness acerta as notas do chart, entao a musica sempre termina
# no mesmo rank (GREAT). Metade da tela de resultado depende do rank:
#
#   * o PAR DE ARTE do `rankText` (`rank_<nome>_vert/hor`), com tamanhos
#     diferentes por rank (LOSS 20x121, GREAT 20x152, ...);
#   * os ATRASOS: `getMusicDelay`, `getFlashDelay`, `getHighscoreDelay`,
#     `getBFDelay` (Scoring.hx L530-604);
#   * a MUSICA e o INTRO: `resultsSHIT` e `resultsEXCELLENT` do bf tem
#     `-intro`, `resultsNORMAL` e `resultsPERFECT` nao tem.
#
# Sem forcar o rank, tudo isso e' testado em UM dos seis casos. O gancho
# `FNF_RANK` (test/ps2harness/ps2h.c) existe so' no binario do harness.
#
# O QUE CADA RODADA COBRA (tudo dentro do run_gameplay_harness.sh)
#
#   arte do rank = o rank forcado; rank na tela no `getFlashDelay` do rank;
#   musica pedida no `getMusicDelay` do rank; intro->loop nos ranks que tem
#   intro e loop direto nos que nao tem; ticks do percentual; audio no audsrv.
#
# Uso: tools/run_resultado_rank_harness.sh [0 1 2 3 4 5]
set -u
REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

RANKS=${*:-"0 1 2 3 4 5"}
FRAMES=${HARNESS_FRAMES:-7600}
FALHAS=0
for R in $RANKS; do
    echo
    echo "== resultado: rank $R forcado =="
    if FNF_RANK="$R" sh tools/run_gameplay_harness.sh "$FRAMES" > "build/rank_$R.log" 2>&1; then
        grep -E "^  (rank na tela|musica do resultado|ticks / audio|arte do rank|contadores|percentual)" "build/rank_$R.log"
        echo "  rank $R: PASSOU"
    else
        echo "  rank $R: FALHOU"
        tail -25 "build/rank_$R.log"
        FALHAS=$((FALHAS + 1))
    fi
done
echo
if [ "$FALHAS" != 0 ]; then
    echo "RANK: FALHOU em $FALHAS rodada(s)"
    exit 1
fi
echo "RANK: PASSOU (arte, atrasos, musica e intro de cada um dos ranks testados)"
