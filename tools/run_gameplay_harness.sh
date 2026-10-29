#!/bin/sh
# Execucao de gameplay do harness: prova que o caminho de ACERTO roda e que a
# musica TERMINA no resultado.
#
# Por que existe separado: o roteiro padrao do harness atravessa o jogo inteiro
# em 1400 quadros e passa ~100 quadros dentro do tutorial -- tempo de sobra para
# desenhar notas, e de menos para qualquer uma delas CHEGAR ao receptor. O
# julgamento (Scoring.hx, vida, combo, placar) nunca era executado no PC; os
# erros nesse caminho so' apareceriam no console.
#
# COMO O ROTEIRO E' GERADO (mudou duas vezes, e a segunda foi uma correcao):
#
#   1a versao, cadencia fixa: o periodo das pressoes e o espacamento das notas
#   tinham um divisor comum grande, entao o erro ficava preso em multiplos de
#   9.600 amostras -- nunca dentro da janela de 7.680. Zero acertos, 45 ghost
#   misses.
#
#   2a versao, cadencia pseudoaleatoria: provava o julgamento (9 acertos), mas
#   23 ghost misses. Cada ghost miss tira 0,08 de vida, entao o jogador MORRIA
#   no meio da musica -- o que ficou visivel quando o game over passou a
#   existir: o teste de fim de musica nunca chegava ao resultado.
#
#   versao atual: as pressoes saem do PROPRIO chart (tools/harness_presses.py),
#   uma por nota do jogador, no quadro em que a nota chega. Nada de aleatorio:
#   o roteiro e' deterministico e o acerto e' o esperado. Algumas notas sao
#   deixadas de fora de proposito para que o caminho de MISS tambem rode.
#
# O roteiro e' em AMOSTRAS, nao em quadros (PS2H_INPUT_AMOSTRAS): o jogo avisa o
# harness em que amostra a musica esta', e a tecla sai quando ela chega ali. Por
# quadro o roteiro erra assim que o tempo por quadro muda -- e muda, porque o
# relogio da musica anda por tempo e os DelayThread do audio tambem andam no
# tempo virtual do harness.
#
# Uso:  tools/run_gameplay_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-5600}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt
CHART=build/harness_assets/FNFPS2/assets/songs/tutorial/normal.chart
# Entrada no tutorial (as tres primeiras cruzes sao titulo -> menu -> story;
# a quarta entra no jogo).
ENTRADA="200:CROSS,420:CROSS,600:CROSS,760:CROSS,820:CROSS,880:CROSS,940:CROSS"

if [ ! -f "$CHART" ]; then
    echo "  FALHA: falta $CHART (rode tools/gen_harness_assets.py)"
    exit 1
fi
TECLAS=$(python3 tools/harness_presses.py "$CHART" --pular 3) || {
    echo "  FALHA: nao consegui gerar o roteiro a partir do chart"; exit 1; }
# grep -c . e nao wc -l: a ultima entrada nao tem quebra de linha, e wc -l
# contava uma a menos do que o roteiro tinha.
NOTAS=$(printf '%s' "$TECLAS" | tr ',' '\n' | grep -c .)
echo "== gameplay: apostando nas notas do chart ($NOTAS pressoes em amostras) =="

rm -f "$LOG"
PS2H_INPUT="$ENTRADA" PS2H_INPUT_AMOSTRAS="$TECLAS" \
    sh tools/run_harness.sh "$FRAMES" || exit 1

if [ ! -f "$LOG" ]; then
    echo "  FALHA: o jogo nao gravou $LOG"
    exit 1
fi

HITS=$(grep -c ',tutorial_note_hit,' "$LOG" 2>/dev/null || true)
[ -z "$HITS" ] && HITS=0
INICIOU=$(awk -F, '$1=="E" && $3=="tutorial_audio_start" {n++} END{print n+0}' "$LOG")
SCORE_MAX=$(awk -F, 'BEGIN{m=-1000000000} $1=="S" && $3==8 {if ($36+0 > m) m = $36+0} END{print m}' "$LOG")
SCORE_MIN=$(awk -F, 'BEGIN{m=1000000000} $1=="S" && $3==8 {if ($36+0 < m) m = $36+0} END{print m}' "$LOG")
SALTOS=$(awk -F, '$1=="S" && $3==8 {if (seen && $36 - prev >= 100) n++; prev=$36; seen=1} END{print n+0}' "$LOG")
CLOCK=$(awk -F, '$1=="S" && $3==8 {if ($27+0 > m) m = $27+0} END{print m+0}' "$LOG")
MISSES=$(awk -F, '$1=="E" && $3=="play_death" {n++} END{print n+0}' "$LOG")

RESULT=$(awk -F, '$1=="S" && $3==9 {n++} END{print n+0}' "$LOG")
RANK=$(awk -F, '$1=="E" && $3=="play_result" {print $4; exit}' "$LOG")
PERC=$(awk -F, '$1=="E" && $3=="play_result" {print $5; exit}' "$LOG")
DRAW=$(awk -F, '$1=="E" && $3=="result_draw" {print $4; exit}' "$LOG")
CDFIM=$(awk -F, '$1=="E" && $3=="countdown_step" && $4==5 {print $2; exit}' "$LOG")
CDPASSOS=$(awk -F, '$1=="E" && $3=="countdown_step" {n++} END{print n+0}' "$LOG")
CDBEAT=$(awk -F, '$1=="E" && $3=="countdown_begin" {print $4; exit}' "$LOG")

echo
echo "  musica iniciada ............. $INICIOU vez(es)"
echo "  relogio maximo da musica ..... $CLOCK amostras"
echo "  acertos julgados ............. $HITS de $NOTAS pressoes"
echo "  placar (minimo..maximo) ...... $SCORE_MIN .. $SCORE_MAX"
echo "  acertos que somaram pontos ... $SALTOS"
echo "  mortes (game over) ........... $MISSES"
echo "  quadros na tela de resultado . $RESULT"
echo "  rank / clear percent ......... ${RANK:-?} / ${PERC:-?}%"
echo "  quads desenhados na tela 9 ... ${DRAW:-?}"
echo "  batida / passos da contagem .. ${CDBEAT:-?} amostras / ${CDPASSOS:-?}"

FALHA=0
[ "$CLOCK" -gt 100000 ] || { echo "  FALHA: o relogio da musica nao andou (o transporte parou)"; FALHA=1; }
[ "$HITS" -ge 20 ]      || { echo "  FALHA: menos de 20 acertos com o roteiro vindo do chart"; FALHA=1; }
[ "$SALTOS" -ge 20 ]    || { echo "  FALHA: os acertos nao somaram pontos"; FALHA=1; }
[ "$SCORE_MAX" -gt 5000 ] || { echo "  FALHA: placar maximo baixo demais para tantos acertos"; FALHA=1; }
# Nenhuma morte: o roteiro acerta as notas, entao a vida sobe. Uma morte aqui
# significa que o alinhamento amostra->quadro saiu errado.
[ "$MISSES" -eq 0 ]     || { echo "  FALHA: o jogador morreu mesmo acertando as notas do chart"; FALHA=1; }

# A tela de resultado tem de aparecer sozinha quando a musica termina, com o
# rank calculado pelo mesmo nucleo que julgou as notas.
[ "$RESULT" -ge 2 ] || { echo "  FALHA: o jogo nao chegou na tela de resultado"; FALHA=1; }
[ -n "$RANK" ]      || { echo "  FALHA: nenhum play_result no log"; FALHA=1; }
[ "$RANK" != "-1" ]  || { echo "  FALHA: rank NONE -- o total de notas do chart chegou zerado no julgamento"; FALHA=1; }
[ "${DRAW:-0}" -gt 0 ] || { echo "  FALHA: a tela de resultado nao desenhou nada"; FALHA=1; }
[ "${CDPASSOS:-0}" -ge 5 ] || { echo "  FALHA: a contagem regressiva nao passou pelos cinco passos"; FALHA=1; }
[ -n "${CDBEAT:-}" ] && [ "$CDBEAT" -gt 0 ] || { echo "  FALHA: a batida da contagem saiu zero"; FALHA=1; }
if [ -n "${CDFIM:-}" ]; then
    awk -F, -v fim="$CDFIM" '$1=="E" && $3=="tutorial_audio_start" {if ($2+0 <= fim+0) {print "  FALHA: a musica comecou antes do fim da contagem"; exit 1} exit 0}' "$LOG" || FALHA=1
else
    echo "  FALHA: a contagem regressiva nunca terminou (sem passo AFTER)"; FALHA=1
fi

if [ "$FALHA" != 0 ]; then
    echo
    echo "GAMEPLAY: FALHOU"
    exit 1
fi

echo
echo "GAMEPLAY: PASSOU (acertos, placar, contagem, fim de musica e resultado no jogo real)"
