#!/bin/sh
# STORY MODE: uma SEMANA INTEIRA, com as musicas em sequencia.
#
# POR QUE ISTO EXISTE
# O Story Mode do port tocava SO' a primeira musica de cada semana e voltava ao
# menu: 20 das 27 musicas existiam e nunca eram alcancadas por ele. No original
# (PlayStatePlaylist.hx + StoryMenuState.selectLevel, L571-598) a semana monta
# uma playlist, toca a primeira, e cada fim de musica faz shift() e carrega a
# proxima SEM passar pela tela de resultado -- que so' aparece no fim da lista,
# com o placar ACUMULADO da campanha (PlayState.hx L3560-3660 e L3885).
#
# O roteiro de teclas tem de ser POR MUSICA (o relogio da musica volta a zero em
# cada uma), entao cada aperto leva o indice da musica: tools/harness_presses.py
# --onda K. Sem isso o roteiro da segunda musica aperta nas horas da primeira,
# vira ghost miss, e o jogador morre antes do teste provar o que queria.
#
# Uso: tools/run_story_harness.sh [semana] [dificuldade] [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

SEMANA=${1:-1}
DIFF=${2:-1}
FRAMES=${3:-0}          # 0 = calcula pelo numero de musicas
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt
DIFFNAME=easy
[ "$DIFF" = 1 ] && DIFFNAME=normal
[ "$DIFF" = 2 ] && DIFFNAME=hard

# As musicas da semana saem do MESMO plano que gera o registro (assets-data),
# na ordem em que o registro as lista.
LISTA=$(python3 - "$SEMANA" <<'PY'
import json, re, sys
semana = int(sys.argv[1])
src = open("tools/gen_song_registry.py").read()
m = re.search(r"LEVEL_ORDER = \[(.*?)\]", src, re.S)
ids = re.findall(r'"([a-z0-9-]+)"', m.group(1))
lid = ids[semana]
d = json.load(open("assets-data/levels/%s.json" % lid))
print(" ".join(d["songs"]))
PY
) || exit 1
[ -n "$LISTA" ] || { echo "  FALHA: semana $SEMANA sem musicas"; exit 1; }
NUM=$(printf '%s\n' $LISTA | grep -c .)
# Quadros por musica: medido em ~3.300 (contagem + 20 s de audio + drenagem do
# anel + a pausa de 90 quadros antes do resultado). 4.000 deixa folga; o teste
# existe para provar a sequencia, nao para cronometrar.
[ "$FRAMES" -gt 0 ] || FRAMES=$((NUM * 4000 + 2000))
echo "== story mode: semana $SEMANA ($NUM musicas: $LISTA) =="

TECLAS=""
K=0
for sid in $LISTA; do
    CHART="build/harness_assets/FNFPS2/assets/songs/$sid/$DIFFNAME.chart"
    [ -f "$CHART" ] || { echo "  FALHA: falta $CHART (rode tools/preparar_sweep.py)"; exit 1; }
    um=$(python3 tools/harness_presses.py "$CHART" --pular 2 --onda "$K" 2>/dev/null) || {
        echo "  FALHA: roteiro de $sid"; exit 1; }
    TECLAS="$TECLAS$um,"
    K=$((K + 1))
done
NOTAS=$(printf '%s' "$TECLAS" | tr ',' '\n' | grep -c .)
echo "  roteiro: $NOTAS pressoes em $NUM musicas"

rm -f "$LOG"
# PS2H_INPUT="0:NONE": o roteiro padrao do harness (que anda pelo menu e aperta
# CIRCLE/CROSS no meio) NAO pode rodar aqui -- esses apertos caem dentro da
# musica, viram ghost miss e derrubam o jogador. Aqui quem aperta sao so' o
# roteiro da campanha (calculado do chart) e um CIRCLE no fim, para sair do
# resultado e provar que a semana volta ao Story Menu.
FNF_START_WEEK="$SEMANA" FNF_START_DIFF="$DIFF" PS2H_FRAMES="$FRAMES" \
    PS2H_INPUT="0:NONE,$((FRAMES - 600)):CIRCLE" PS2H_INPUT_AMOSTRAS="$TECLAS" \
    sh tools/run_harness.sh "$FRAMES" > build/story_harness.out 2>&1
rc=$?
tail -4 build/story_harness.out
[ $rc -eq 0 ] || { echo "  FALHA: o harness nao passou"; exit 1; }

conta() { awk -F, -v n="$1" '$1=="E" && $3==n {c++} END{print c+0}' "$LOG"; }
NEXT=$(conta story_next)
SONGS=$(conta play_song)
RES=$(conta result_enter)
ACERTOS=$(awk -F, '$1=="E" && $3=="play_result_tallies" {print $4; exit}' "$LOG")
SCORE1=$(awk -F, '$1=="E" && $3=="play_result_score" {print $4; exit}' "$LOG")

echo
echo "  musicas tocadas ............ $SONGS"
echo "  vezes que emendou .......... $NEXT (esperado $((NUM - 1)))"
echo "  telas de resultado ......... $RES (esperado 1, so' no fim da semana)"
echo "  placar da 1a musica ........ ${SCORE1:-?}"

FALHA=0
[ "$SONGS" -ge "$NUM" ] || { echo "  FALHA: a semana nao tocou as $NUM musicas (tocou $SONGS)"; FALHA=1; }
[ "$NEXT" -ge $((NUM - 1)) ] || { echo "  FALHA: a semana nao emendou as musicas ($NEXT emendas)"; FALHA=1; }
[ "$RES" -eq 1 ] || { echo "  FALHA: o resultado apareceu $RES vez(es); so' devia aparecer no fim"; FALHA=1; }
# Cada musica emendada tem de ser outra musica, nao a mesma de novo.
DISTINTOS=$(awk -F, '$1=="E" && $3=="play_song" {print $4}' "$LOG" | sort -u | grep -c .)
[ "$DISTINTOS" -eq "$NUM" ] || {
    echo "  FALHA: $DISTINTOS musica(s) diferente(s) tocada(s), esperado $NUM"; FALHA=1; }
if ! grep -q "state,7," "$LOG"; then
    echo "  FALHA: a semana nao voltou ao Story Menu no fim"; FALHA=1
fi
[ $FALHA -eq 0 ] || exit 1
echo
echo "STORY: PASSOU ($NUM musicas em sequencia, resultado so' no fim, de volta ao Story Menu)"
