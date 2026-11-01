#!/bin/sh
# Prova, no JOGO DE VERDADE, que o combo/drop da GF chega a tocar.
#
# POR QUE UM HARNESS SEPARADO
# `combo50`/`combo200`/`drop70` sao animacoes de 8 dos 46 personagens (gf e nene,
# com as variantes) e so' tocam quando o jogador ACERTA a nota 50/200 de uma
# sequencia ou QUEBRA um combo que ainda alcanca o degrau (drop70). O harness
# comum nao chega la': o roteiro dele toca o tutorial (29 acertos) e o sweep toca
# 20 s de cada musica, o que da' menos de 50 notas seguidas. Sem este script, o
# caminho ficaria provado so' no teste de unidade -- e a ligacao com o jogo
# (quem chama cc_combo/cc_drop, com que numero) nao estaria provada.
#
# COMO
#   1. gera o stem de 70 s e o chart da `2hot` hard (a musica mais densa: 585
#      notas do jogador);
#   2. roda com as pressoes vindas do proprio chart, deixando 8 notas de fora
#      (--pular 8): a primeira nota pulada cai depois de ~73 acertos, que e' o
#      combo que quebra e faz a GF tocar o `drop70`;
#   3. exige no log: `char_combo_anims` 2/1 no papel da GF (o dado), um
#      `char_combo` no 50 com julgamento sick (4) e um `char_combo_drop` no 73.
#
# O stem volta a 20 s no fim, que e' o que o sweep espera.
#
# Uso:  tools/run_combo_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-6000}
ROOT=build/harness_assets
BIN=build/fnf_harness
LOG=$ROOT/FNFPS2/FNF_COLHEITA.txt
SID=2hot
DIFF=hard

if [ ! -x "$BIN" ]; then
    echo "== compilando o harness (nao existia) =="
    sh tools/run_harness.sh 30 >/dev/null 2>&1 || { echo "FALHA: nao consegui compilar o harness"; exit 1; }
fi

restaurar() {
    # Devolve o stem de 20 s, que e' a configuracao do sweep.
    python3 tools/preparar_sweep.py --forcar --segundos 20 --musicas "$SID" >/dev/null 2>&1 || true
}
trap restaurar EXIT INT TERM

echo "== gerando chart e stem de 70 s para $SID/$DIFF =="
python3 tools/preparar_sweep.py --forcar --segundos 70 --musicas "$SID" >/dev/null 2>&1 || {
    echo "FALHA: nao consegui gerar os assets do sweep"; exit 1; }
CHART=$ROOT/FNFPS2/assets/songs/$SID/$DIFF.chart
[ -f "$CHART" ] || { echo "FALHA: $CHART nao existe"; exit 1; }

TECLAS=$(python3 tools/harness_presses.py "$CHART" --pular 8 2>/dev/null) || TECLAS=""
[ -n "$TECLAS" ] || { echo "FALHA: nenhuma tecla gerada do chart"; exit 1; }

echo "== rodando $FRAMES quadros em $SID/$DIFF com as pressoes do chart (8 notas de fora) =="
rm -f "$LOG"
FNF_START_SONG="$SID" FNF_START_DIFF=2 PS2H_FRAMES="$FRAMES" PS2H_ROOT="$ROOT" \
    PS2H_WATCHDOG=600 PS2H_INPUT="0:NONE" PS2H_INPUT_AMOSTRAS="$TECLAS" \
    timeout 600 "$BIN" >/tmp/fnf_combo.out 2>&1
rc=$?
if ! grep -q "RESULTADO: PASSOU" /tmp/fnf_combo.out; then
    echo "FALHA: a execucao do harness nao passou (rc=$rc)"; tail -5 /tmp/fnf_combo.out; exit 1
fi
if [ ! -f "$LOG" ]; then
    echo "FALHA: o jogo nao escreveu o log em $LOG"; exit 1
fi

n_anims=$(awk -F, '$1=="E" && $3=="char_combo_anims" && $4==2 && $5==1 {n++} END{print n+0}' "$LOG")
n_combo=$(awk -F, '$1=="E" && $3=="char_combo" && $4==50 && $5==4 {n++} END{print n+0}' "$LOG")
n_drop=$(awk -F, '$1=="E" && $3=="char_combo_drop" && $4==73 {n++} END{print n+0}' "$LOG")
hits=$(awk -F, '$1=="E" && $3=="tutorial_note_hit" {n++} END{print n+0}' "$LOG")

echo "  acertos julgados ......... $hits"
echo "  GF com 2 combo(s)/1 drop .. $n_anims  (o dado: combo50, combo200, drop70)"
echo "  combo50 tocado (sick) .... $n_combo"
echo "  drop70 tocado no combo 73  $n_drop"
echo
falhou=0
[ "${n_anims:-0}" -ge 1 ] || { echo "  FALHA: a GF nao carregou as animacoes de combo/drop"; falhou=1; }
[ "${n_combo:-0}" -ge 1 ] || { echo "  FALHA: combo50 nao tocou no acerto 50"; falhou=1; }
[ "${n_drop:-0}" -ge 1 ] || { echo "  FALHA: drop70 nao tocou quando o combo 73 quebrou"; falhou=1; }
[ "${hits:-0}" -ge 60 ] || { echo "  FALHA: so' $hits acertos -- o roteiro nao chegou ao combo 50"; falhou=1; }
# e o contrario: um combo que NAO e' degrau nao pode tocar animacao nenhuma
n_outro=$(awk -F, '$1=="E" && $3=="char_combo" && $4!=50 && $4!=200 {n++} END{print n+0}' "$LOG")
[ "${n_outro:-0}" -eq 0 ] || { echo "  FALHA: combo tocou num numero que nao e' degrau ($n_outro)"; falhou=1; }
if [ "$falhou" -ne 0 ]; then
    echo "COMBO: FALHOU"
    exit 1
fi
echo "COMBO: PASSOU (o combo/drop do dado chega ao jogo e toca na hora certa)"
