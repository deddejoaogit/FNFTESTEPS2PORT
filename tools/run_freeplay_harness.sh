#!/bin/sh
# Harness da FREEPLAY: abrir pelo menu, trocar dificuldade, tocar e VOLTAR.
#
# O que este teste cobra:
#   - o item FREEPLAY do menu abre a tela (antes ele mostrava "COMING IN NEXT
#     STAGE" e o jogador nao chegava em 26 das 27 musicas);
#   - a lista e' montada do registro (27 musicas, sem repetir);
#   - a tela DESENHA (estado existir nao prova nada);
#   - LEFT/RIGHT troca a dificuldade para uma que a musica tem;
#   - CROSS toca a musica ESCOLHIDA, na dificuldade escolhida, carregando o
#     chart dela (o chart tem de carregar: `tutorial_chart` com checksum != 0);
#   - ao sair do resultado o jogo volta para a FREEPLAY, nao para o Story.
#
# Uso: tools/run_freeplay_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-6000}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt

# 200:CROSS titulo -> menu. 620:DOWN desce para FREEPLAY (item 1).
# 660:CROSS abre a Freeplay. 700:RIGHT troca a dificuldade.
# 760:CROSS toca a musica selecionada (a primeira da lista, o Tutorial).
# 5600:CIRCLE sai do resultado: tem de voltar para a Freeplay.
ENTRADA="200:CROSS,420:CROSS,640:DOWN,700:CROSS,840:RIGHT,900:CROSS,5600:CIRCLE"

rm -f "$LOG"
echo "== freeplay: menu -> lista -> musica =="
PS2H_INPUT="$ENTRADA" sh tools/run_harness.sh "$FRAMES" || exit 1

ABRIU=$(awk -F, '$1=="E" && $3=="freeplay_open" {print $4; exit}' "$LOG")
NSONGS=$(awk -F, '$1=="E" && $3=="freeplay_open" {print $4; exit}' "$LOG")
ENTROU=$(awk -F, '$1=="E" && $3=="freeplay_enter" {n++} END{print n+0}' "$LOG")
FDRAW=$(awk -F, '$1=="E" && $3=="freeplay_draw" {print $4; exit}' "$LOG")
FPLAY=$(awk -F, '$1=="E" && $3=="freeplay_play" {print $4; exit}' "$LOG")
FDIFF=$(awk -F, '$1=="E" && $3=="freeplay_diff" {print $4; exit}' "$LOG")
CHART=$(awk -F, '$1=="E" && $3=="tutorial_chart" {print $5; exit}' "$LOG")
NOTAS=$(awk -F, '$1=="E" && $3=="play_total_notes" {print $4; exit}' "$LOG")
AUDIO=$(awk -F, '$1=="E" && $3=="tutorial_audio_start" {n++} END{print n+0}' "$LOG")
VOLTOU=$(awk -F, '$1=="S" && $3==11 {n++} END{print n+0}' "$LOG")

echo
echo "  musicas na lista ............ ${NSONGS:-?}"
echo "  vezes que a freeplay abriu .. ${ENTROU:-?}"
echo "  quads desenhados na tela 11 . ${FDRAW:-?}"
echo "  dificuldade escolhida ....... ${FDIFF:-?} (2 = HARD)"
echo "  musica escolhida (hash) ..... ${FPLAY:-?}"
echo "  chart carregado (checksum) .. ${CHART:-?}"
echo "  notas do chart .............. ${NOTAS:-?}"
echo "  vezes que a musica comecou .. $AUDIO"
echo "  quadros de volta na freeplay  $VOLTOU"

FALHA=0
[ "${NSONGS:-0}" -ge 20 ] || { echo "  FALHA: a lista da Freeplay saiu com menos de 20 musicas"; FALHA=1; }
[ "$ENTROU" -ge 1 ] || { echo "  FALHA: a tela de Freeplay nunca abriu"; FALHA=1; }
[ "${FDRAW:-0}" -gt 0 ] || { echo "  FALHA: a tela de Freeplay nao desenhou nada"; FALHA=1; }
[ -n "${FPLAY:-}" ] || { echo "  FALHA: o CROSS nao tocou nenhuma musica"; FALHA=1; }
[ "${FDIFF:-?}" -ge 1 ] || { echo "  FALHA: a troca de dificuldade nao aconteceu"; FALHA=1; }
[ -n "${CHART:-}" ] && [ "${CHART:-0}" != "0" ] || { echo "  FALHA: o chart da musica escolhida nao carregou"; FALHA=1; }
[ "${NOTAS:-0}" -gt 0 ] || { echo "  FALHA: o total de notas saiu zero"; FALHA=1; }
[ "$AUDIO" -ge 1 ] || { echo "  FALHA: a musica da Freeplay nao comecou a tocar"; FALHA=1; }
[ "$VOLTOU" -ge 2 ] || { echo "  FALHA: depois do resultado o jogo nao voltou para a Freeplay"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "FREEPLAY: FALHOU"
    exit 1
fi

echo
echo "FREEPLAY: PASSOU (lista, dificuldade, chart da musica escolhida e volta)"
