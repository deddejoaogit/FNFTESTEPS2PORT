#!/bin/sh
# Harness do GAME OVER: morrer de proposito, ver a tela, e reiniciar.
#
# Por que existe separado do teste de gameplay: la' o roteiro ACERTA as notas
# (elas vem do proprio chart), a vida sobe e a musica termina. Aqui o roteiro nao
# aperta NADA depois de entrar no jogo, entao as notas passam e viram MISS: cada
# miss tira 0,08 de vida (Scoring.hx/Constants.hx) e a 13a nota o jogador morre.
# E' o caminho que o harness nunca exercitou -- antes do game over existir, a
# vida chegava a zero e o jogo seguia como se nada tivesse acontecido.
#
# Duas passadas de proposito:
#   1) sem CROSS, para DESCOBRIR o quadro da morte pelo log;
#   2) com CROSS 120 quadros depois da morte (o original bloqueia entrada por
#      1 s), para provar que o reinicio volta a tocar a musica.
# O roteiro e' deterministico, entao as duas passadas sao comparaveis.
#
# Uso: tools/run_gameover_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-5000}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt
ENTRADA="200:CROSS,420:CROSS,600:CROSS,760:CROSS,820:CROSS,880:CROSS,940:CROSS"

rm -f "$LOG"
echo "== game over: passada 1 (morrer sem apertar nada) =="
PS2H_INPUT="$ENTRADA" sh tools/run_harness.sh "$FRAMES" >/dev/null 2>&1 || {
    echo "  FALHA: o harness nao terminou a passada 1"; exit 1; }

# O formato do evento e' "E,<carimbo>,<nome>,<a>,<b>": o QUADRO esta' no campo
# 4 quando o primeiro parametro do evento e' o quadro (gameover_enter) e no 5
# quando nao e' (play_death leva o numero de misses no campo 4).
MORTE=$(awk -F, '$1=="E" && $3=="gameover_enter" {print $4; exit}' "$LOG")
if [ -z "$MORTE" ]; then
    echo "  FALHA: o jogador nao morreu -- nao houve gameover_enter em $LOG"
    grep -m3 ',play_death,' "$LOG" || true
    exit 1
fi
echo "  morte no quadro ............ $MORTE"

RETRY=$((MORTE + 120))
rm -f "$LOG"
echo "== game over: passada 2 (CROSS 120 quadros depois da morte) =="
PS2H_INPUT="$ENTRADA,${RETRY}:CROSS" sh tools/run_harness.sh "$FRAMES" >/dev/null 2>&1 || {
    echo "  FALHA: o harness nao terminou a passada 2"; exit 1; }

DEATH=$(awk -F, '$1=="E" && $3=="play_death" {print $5; exit}' "$LOG")
GENTER=$(awk -F, '$1=="E" && $3=="gameover_enter" {print $4; exit}' "$LOG")
GDRAW=$(awk -F, '$1=="E" && $3=="gameover_draw" {print $4; exit}' "$LOG")
GOUT=$(awk -F, '$1=="E" && $3=="gameover_restart" {print $4; exit}' "$LOG")
GSTATE=$(awk -F, '$1=="S" && $3==10 {n++} END{print n+0}' "$LOG")
CINCOS=$(awk -F, '$1=="E" && $3=="countdown_step" && $4==5 {n++} END{print n+0}' "$LOG")
AUDIOS=$(awk -F, '$1=="E" && $3=="tutorial_audio_start" {n++} END{print n+0}' "$LOG")
MISSES=$(awk -F, '$1=="E" && $3=="play_death" {print $4; exit}' "$LOG")

echo
echo "  quadro da morte ............. ${DEATH:-?}"
echo "  tela de game over desde ..... ${GENTER:-?}"
echo "  quadros em ui_state 10 ...... $GSTATE"
echo "  quads desenhados na tela 10 . ${GDRAW:-?}"
echo "  misses na morte ............. ${MISSES:-?}"
echo "  reinicio em ................. ${GOUT:-?}"
echo "  vezes que a musica comecou .. $AUDIOS"
echo "  contagens terminadas ........ $CINCOS"

FALHA=0
[ -n "$DEATH" ]  || { echo "  FALHA: nenhum play_death no log"; FALHA=1; }
[ -n "$GENTER" ] || { echo "  FALHA: a tela de game over nunca abriu"; FALHA=1; }
[ "$GSTATE" -ge 30 ] || { echo "  FALHA: ui_state 10 durou menos de 30 quadros"; FALHA=1; }
[ "${GDRAW:-0}" -gt 0 ] || { echo "  FALHA: a tela de game over nao desenhou nada"; FALHA=1; }
[ -n "$GOUT" ]   || { echo "  FALHA: o CROSS nao reiniciou a musica"; FALHA=1; }
[ "$AUDIOS" -ge 2 ] || { echo "  FALHA: a musica nao voltou a tocar depois do reinicio"; FALHA=1; }
[ "$CINCOS" -ge 2 ] || { echo "  FALHA: a contagem regressiva nao recomecou"; FALHA=1; }
# Uma morte sem nota perdida nao faz sentido: a vida so' cai por miss/ghost.
[ "${MISSES:-0}" -ge 13 ] || { echo "  FALHA: morte com menos de 13 misses (a vida nao caiu como devia)"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "GAME OVER: FALHOU"
    exit 1
fi

echo
echo "GAME OVER: PASSOU (morte por vida zero, tela desenhada, reinicio tocando de novo)"
