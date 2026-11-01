#!/bin/sh
# Harness do PRACTICE MODE (PlayState.isPracticeMode).
#
# O que ele cobra, e por que cada item importa:
#   - o menu da pausa tem CINCO entradas com o modo desligado
#     (Resume / Restart Song / Change Difficulty / Enable Practice Mode /
#     Exit to Menu) e QUATRO depois de ligar -- a entrada carrega
#     `filter: () -> !isPracticeMode` no original, entao ela some;
#   - ligar o modo NAO fecha a pausa (enablePracticeMode so' regenera o menu);
#   - com o modo ligado a vida chega a HEALTH_MIN e o jogo NAO morre
#     (`PlayState.hx` L1255: `if (health <= HEALTH_MIN && !isPracticeMode)`);
#   - a musica continua ate' o fim e o resultado aparece (no original o
#     Practice invalida o highscore, nao a partida).
#
# A musica roda SEM roteiro de notas de proposito: errar tudo e' o que leva a
# vida ao chao -- que e' justamente o que o Practice Mode protege.
#
# Uso:  tools/run_practice_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-5800}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt

# 200..940 entram no tutorial; 1200 abre a pausa; 1260..1272 descem ate a
# quinta entrada (DOWN x3) e 1280 confirma; 1400 fecha com START.
ENTRADA="200:CROSS,420:CROSS,600:CROSS,760:CROSS,820:CROSS,880:CROSS,940:CROSS,1200:START,1260:DOWN,1264:DOWN,1268:DOWN,1280:CROSS,1400:START"

rm -f "$LOG"
PS2H_INPUT="$ENTRADA" sh tools/run_harness.sh "$FRAMES" >/dev/null 2>&1 || {
    echo "  FALHA: o harness nao terminou"; exit 1; }

MENU1=$(awk -F, '$1=="E" && $3=="pause_menu" {print $4; exit}' "$LOG")
MENU2=$(awk -F, '$1=="E" && $3=="pause_menu" {n=$4} END{print n+0}' "$LOG")
PRAT=$(awk -F, '$1=="E" && $3=="pause_practice" {print $4; exit}' "$LOG")
MORTES=$(grep -ac ',play_death,' "$LOG" 2>/dev/null || true)
[ -z "$MORTES" ] && MORTES=0
RESULT=$(awk -F, '$1=="S" && $3==9 {n++} END{print n+0}' "$LOG")
ACERTOS=$(grep -ac ',tutorial_note_hit,' "$LOG" 2>/dev/null || true)
[ -z "$ACERTOS" ] && ACERTOS=0
ZERO=$(awk -F, '$1=="E" && $3=="vida_zero" {print $4; exit}' "$LOG")

echo
echo "  entradas no menu (antes/depois) ... ${MENU1:-?} / ${MENU2:-?}"
echo "  pause_practice (dead, quadro) ..... ${PRAT:-nao ligou}"
echo "  mortes (play_death) ............... $MORTES"
echo "  vida_zero (dead, quadro) .......... ${ZERO:-nao zerou}"
echo "  quadros na tela de resultado ...... $RESULT"

FALHA=0
[ "${MENU1:-0}" = "5" ] || { echo "  FALHA: o menu na pausa com Practice desligado tem de ter 5 entradas"; FALHA=1; }
[ -n "$PRAT" ] || { echo "  FALHA: nao foi possivel ligar o Practice Mode pelo menu"; FALHA=1; }
[ "${PRAT:-1}" = "0" ] || { echo "  FALHA: pause_practice com dead != 0 (o jogo ja' estava morto)"; FALHA=1; }
[ "${MENU2:-0}" = "4" ] || { echo "  FALHA: depois de ligar, a entrada 'Enable Practice Mode' tem de sumir (4 entradas)"; FALHA=1; }
[ "$MORTES" -eq 0 ] || { echo "  FALHA: o jogo morreu $MORTES vez(es) no Practice Mode"; FALHA=1; }
# A vida tem de ter CHEGADO ao chao sem ninguem apertar nada -- e `dead` (campo
# 1 do evento) tem de continuar 0, que e' a unica diferenca de comportamento do
# Practice Mode.
[ -n "$ZERO" ] || { echo "  FALHA: a vida nunca chegou a HEALTH_MIN ($ACERTOS acertos)"; FALHA=1; }
[ "${ZERO:-1}" = "0" ] || { echo "  FALHA: vida_zero com dead=1 -- o Practice Mode nao protegeu"; FALHA=1; }
[ "$RESULT" -ge 2 ] || { echo "  FALHA: a musica nao chegou ao resultado depois de zerar a vida"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "PRACTICE: FALHOU"
    exit 1
fi
echo
echo "PRACTICE: PASSOU (menu de 5 -> 4, vida zerada sem morrer, musica ate' o fim)"
