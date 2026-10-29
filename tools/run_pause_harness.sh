#!/bin/sh
# Harness da PAUSA (PauseSubState.hx): abrir, congelar, e voltar de tres jeitos.
#
# O que este teste cobra, e por que cada item importa:
#   - a pausa ABRE no START e o jogo CONGELA de verdade (o relogio da musica nao
#     anda enquanto ela esta' aberta -- "pausado" que continua tocando nao e'
#     pausa);
#   - a tela desenha (o estado existir nao prova nada);
#   - Resume volta a tocar do MESMO ponto (nem antes, nem do zero);
#   - Restart Song rebobina (a contagem regressiva recomeca);
#   - Change Difficulty recarrega o chart na dificuldade escolhida.
#
# Duas passadas: a primeira testa Resume e deixa a musica terminar (o caminho
# normal); a segunda testa troca de dificuldade e reinicio.
#
# Uso: tools/run_pause_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-5600}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt
ENTRADA="200:CROSS,420:CROSS,600:CROSS,760:CROSS,820:CROSS,880:CROSS,940:CROSS"

PAUSA=1200
VOLTA=1320
CHART=build/harness_assets/FNFPS2/assets/songs/tutorial/normal.chart

# O roteiro das notas e' em AMOSTRAS (o jogo avisa o relogio ao harness), entao
# a pausa nao precisa de compensacao nenhuma: enquanto ela esta' aberta o relogio
# nao anda, e as notas seguintes continuam esperando. E e' justamente isso que o
# teste cobra no fim -- se o Resume voltasse de outro ponto do audio, as notas
# depois da pausa seriam perdidas e o resultado sairia com menos acertos.
TECLAS=$(python3 tools/harness_presses.py "$CHART" --pular 3) || {
    echo "  FALHA: nao consegui gerar o roteiro a partir do chart"; exit 1; }

rm -f "$LOG"
echo "== pausa: passada 1 (START pausa, START volta) =="
PS2H_INPUT="$ENTRADA,${PAUSA}:START,${VOLTA}:START" PS2H_INPUT_AMOSTRAS="$TECLAS" \
    sh tools/run_harness.sh "$FRAMES" >/dev/null 2>&1 || {
    echo "  FALHA: o harness nao terminou a passada 1"; exit 1; }

ABRIU=$(awk -F, '$1=="E" && $3=="pause_open" {print $4; exit}' "$LOG")
PDRAW=$(awk -F, '$1=="E" && $3=="pause_draw" {print $4; exit}' "$LOG")
FECHOU=$(awk -F, '$1=="E" && $3=="pause_close" {print $4; exit}' "$LOG")
# Relogio no quadro em que a pausa abriu e no ultimo quadro ainda pausado.
CLK0=$ABRIU
CLKP=$(awk -F, -v p="$PAUSA" -v v="$VOLTA" '$1=="S" && $3==8 && $2>=p && $2<v {print $27}' "$LOG" | sort -n | uniq | tr '\n' ' ')
CLKP_ULT=$(echo "$CLKP" | tr ' ' '\n' | grep -c . )
CLKP_UNIQ=$(echo "$CLKP" | tr ' ' '\n' | grep . | sort -u | wc -l | tr -d ' ')
CLKFIM=$(awk -F, '$1=="S" && $3==8 {if ($27+0 > m) m=$27+0} END{print m+0}' "$LOG")
RESULT=$(awk -F, '$1=="S" && $3==9 {n++} END{print n+0}' "$LOG")
HITS=$(grep -c ',tutorial_note_hit,' "$LOG" 2>/dev/null || true)
[ -z "$HITS" ] && HITS=0
RANK=$(awk -F, '$1=="E" && $3=="play_result" {print $4; exit}' "$LOG")

echo
echo "  relogio ao pausar ............ $CLK0"
echo "  quadros pausados / valores ... $CLKP_ULT / $CLKP_UNIQ relogio(s) distinto(s)"
echo "  quads na sobreposicao ........ ${PDRAW:-?}"
echo "  relogio ao voltar ............ ${FECHOU:-?}"
echo "  relogio maximo ............... $CLKFIM"
echo "  quadros na tela de resultado . $RESULT"
echo "  acertos depois da pausa ..... $HITS"
echo "  rank no resultado ............ ${RANK:-?}"

FALHA=0
[ -n "$ABRIU" ]  || { echo "  FALHA: o START nao abriu a pausa"; FALHA=1; }
[ "${PDRAW:-0}" -gt 0 ] || { echo "  FALHA: a sobreposicao da pausa nao desenhou nada"; FALHA=1; }
[ -n "$FECHOU" ] || { echo "  FALHA: a pausa nao fechou no segundo START"; FALHA=1; }
# Congelado: durante a pausa o relogio tem de ter UM unico valor.
[ "$CLKP_UNIQ" -eq 1 ] || { echo "  FALHA: o relogio andou durante a pausa ($CLKP_UNIQ valores)"; FALHA=1; }
# Resume volta do mesmo ponto: o relogio ao fechar e' o mesmo do congelado.
[ "${FECHOU:-0}" = "$CLK0" ] || { echo "  FALHA: o Resume nao voltou do ponto em que parou"; FALHA=1; }
# E depois de voltar, a musica segue ate o fim.
[ "$CLKFIM" -gt "$CLK0" ] || { echo "  FALHA: a musica nao continuou depois do Resume"; FALHA=1; }
[ "$RESULT" -ge 2 ] || { echo "  FALHA: depois da pausa a musica nao chegou ao resultado"; FALHA=1; }
# O Resume tem de voltar do MESMO ponto: se voltasse de outro lugar do audio, as
# notas depois da pausa seriam perdidas e os acertos cairiam. O roteiro acerta 29
# notas (3 ficam para tras de proposito); exigir 25 deixa folga para um erro de
# arredondamento de quadro e nenhuma para um audio adiantado/atrasado.
[ "$HITS" -ge 25 ] || { echo "  FALHA: poucos acertos depois do Resume ($HITS) -- o audio nao voltou do ponto certo"; FALHA=1; }
[ "${RANK:-?}" != "-1" ] || { echo "  FALHA: rank NONE no resultado"; FALHA=1; }

rm -f "$LOG"
echo
echo "== pausa: passada 2 (Change Difficulty e Restart Song) =="
# START abre; DOWN DOWN CROSS entra em Change Difficulty; DOWN CROSS escolhe a
# segunda dificuldade da lista; depois START de novo pausa; DOWN CROSS reinicia.
PS2H_INPUT="$ENTRADA,1000:START,1040:DOWN,1044:DOWN,1048:CROSS,1080:DOWN,1084:CROSS,1300:START,1340:DOWN,1344:CROSS" \
    sh tools/run_harness.sh 2000 >/dev/null 2>&1 || {
    echo "  FALHA: o harness nao terminou a passada 2"; exit 1; }

DIFF=$(awk -F, '$1=="E" && $3=="pause_change_difficulty" {print $4; exit}' "$LOG")
REST=$(awk -F, '$1=="E" && $3=="pause_restart" {print $4; exit}' "$LOG")
CARDS=$(awk -F, '$1=="E" && $3=="tutorial_chart" {n++} END{print n+0}' "$LOG")
CINCOS=$(awk -F, '$1=="E" && $3=="countdown_step" && $4==5 {n++} END{print n+0}' "$LOG")

echo "  dificuldade escolhida ........ ${DIFF:-?}"
echo "  reinicio pela pausa em ....... ${REST:-?}"
echo "  charts carregados ............ $CARDS"
echo "  contagens completas .......... $CINCOS"

[ -n "$DIFF" ] || { echo "  FALHA: a troca de dificuldade nao aconteceu"; FALHA=1; }
[ "$CARDS" -ge 2 ] || { echo "  FALHA: o chart nao foi recarregado na nova dificuldade"; FALHA=1; }
[ -n "$REST" ] || { echo "  FALHA: o Restart Song nao reiniciou"; FALHA=1; }
[ "$CINCOS" -ge 2 ] || { echo "  FALHA: a contagem regressiva nao recomecou"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "PAUSA: FALHOU"
    exit 1
fi

echo
echo "PAUSA: PASSOU (abre, congela, desenha, Resume, Restart e Change Difficulty)"
