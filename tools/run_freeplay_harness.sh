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
CAP=$(awk -F, '$1=="E" && $3=="freeplay_capsula" {print $4","$5; exit}' "$LOG")
TIT=$(awk -F, '$1=="E" && $3=="freeplay_titulos" {print $4","$5; exit}' "$LOG")
CAPS=$(awk -F, '$1=="E" && $3=="freeplay_capsulas" {print $4; exit}' "$LOG")
# O FUNDO da tela (`backingImage`): abriu (1) e o x do primeiro desenho, em
# decimos de px. Ele nasce em -width e chega a 0 em 700 ms com quintOut.
FUNDO=$(awk -F, '$1=="E" && $3=="freeplay_fundo" {print $4","$5; exit}' "$LOG")
FUNDOX=$(awk -F, '$1=="E" && $3=="freeplay_fundo_desenho" {print $4","$5; exit}' "$LOG")

echo
echo "  musicas na lista ............ ${NSONGS:-?}"
echo "  vezes que a freeplay abriu .. ${ENTROU:-?}"
echo "  quads desenhados na tela 11 . ${FDRAW:-?}"
echo "  dificuldade escolhida ....... ${FDIFF:-?} (2 = HARD)"
echo "  musica escolhida (hash) ..... ${FPLAY:-?}"
echo "  chart carregado (checksum) .. ${CHART:-?}"
echo "  notas do chart .............. ${NOTAS:-?}"
echo "  vezes que a musica comecou .. $AUDIO"
echo "  capsula (abriu, bytes) ...... ${CAP:-?}"
echo "  titulos (bytes, falhas) ..... ${TIT:-?}"
echo "  fundo (abriu, bytes) ........ ${FUNDO:-?}   (574x360 em 16 bpp = 413280 B)"
echo "  fundo: x do 1. desenho ...... ${FUNDOX:-?}   (decimos de px e ms; entra de -574 a 0 em 700 ms)"
echo "  capsulas desenhadas ......... ${CAPS:-?}"
echo "  quadros de volta na freeplay  $VOLTOU"

FALHA=0
[ "${NSONGS:-0}" -ge 20 ] || { echo "  FALHA: a lista da Freeplay saiu com menos de 20 musicas"; FALHA=1; }
[ "$ENTROU" -ge 1 ] || { echo "  FALHA: a tela de Freeplay nunca abriu"; FALHA=1; }
[ "${FDRAW:-0}" -gt 0 ] || { echo "  FALHA: a tela de Freeplay nao desenhou nada"; FALHA=1; }
# A ARTE DAS CAPSULAS: o pacote tem de abrir (nao basta a tela desenhar texto) e
# os 27 titulos tem de estar na VRAM sem uma falha -- e' o que prova que o
# caminho do empacotador -> pendrive -> VRAM funciona ponta a ponta.
[ "${CAP%%,*}" = "1" ] || { echo "  FALHA: a capsula da Freeplay nao abriu (${CAP:-sem evento})"; FALHA=1; }
[ "${TIT##*,}" = "0" ] || { echo "  FALHA: titulo da Freeplay faltando (${TIT:-sem evento})"; FALHA=1; }
[ "${TIT%%,*}" -ge 100000 ] 2>/dev/null || { echo "  FALHA: os titulos somam menos de 100.000 B (${TIT:-?})"; FALHA=1; }
[ "${CAPS:-0}" -ge 5 ] || { echo "  FALHA: menos de 5 capsulas desenhadas (${CAPS:-0})"; FALHA=1; }
# O FUNDO: o pacote tem de abrir com o tamanho do header (w*h*2) e o desenho tem
# de comecar FORA da tela (x negativo), senao o tween de entrada do original
# (`x = -width` -> 0 em 0,7 s) nao esta' acontecendo.
[ "${FUNDO%%,*}" = "1" ] || { echo "  FALHA: o fundo da Freeplay nao abriu (${FUNDO:-sem evento})"; FALHA=1; }
[ "${FUNDO##*,}" = "413280" ] || { echo "  FALHA: o fundo abriu com ${FUNDO##*,} B e o header manda 413280"; FALHA=1; }
if [ -z "${FUNDOX:-}" ]; then
    echo "  FALHA: o fundo nunca chegou a ser desenhado"; FALHA=1
else
    FX=${FUNDOX%%,*}
    [ "$FX" -lt 0 ] || { echo "  FALHA: o fundo apareceu em x=$FX decimos (o original entra de -574)"; FALHA=1; }
    [ "$FX" -ge -5740 ] || { echo "  FALHA: o fundo comecou em x=$FX decimos, antes de -574 px"; FALHA=1; }
fi
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
