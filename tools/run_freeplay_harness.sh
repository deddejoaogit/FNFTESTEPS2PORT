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

FRAMES=${1:-7000}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt

# 200:CROSS titulo -> menu. 420:CROSS menu (STORY) nao: 640:DOWN desce para
# FREEPLAY (item 1). 700:CROSS abre a Freeplay. 840:RIGHT troca a dificuldade.
# 900:CROSS toca a musica selecionada (a primeira da lista, o Tutorial).
# 1200..1280: START abre a pausa, DOWN x3 e CROSS LIGAM o Practice Mode -- sem
# ele o roteiro sem notas morre no meio da musica (game over) e o RESULTADO,
# que e' o que faz o DJ fazer o fist pump, nunca apareceria. 1400:START volta
# ao jogo. 5400:CIRCLE sai do resultado: tem de voltar para a Freeplay.
ENTRADA="200:CROSS,420:CROSS,640:DOWN,700:CROSS,840:RIGHT,900:CROSS,1200:START,1260:DOWN,1264:DOWN,1268:DOWN,1280:CROSS,1400:START,5750:CIRCLE"

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
# O DJ da Freeplay (`AnimateAtlasFreeplayDJ`): abriu (1, bytes por quadro),
# desenhou, e -- na VOLTA do resultado -- quais estados ele passou.
DJ=$(awk -F, '$1=="E" && $3=="freeplay_dj" {print $4","$5; exit}' "$LOG")
DJFALHA=$(awk -F, '$1=="E" && $3=="freeplay_dj_quadro_falhou" {print $4; exit}' "$LOG")
DJTAM=$(awk -F, '$1=="E" && $3=="freeplay_dj_tamanho" {print $4","$5; exit}' "$LOG")
DJDRW=$(awk -F, '$1=="E" && $3=="freeplay_dj_desenho" {n++} END{print n+0}' "$LOG")
DJEST=$(awk -F, '$1=="E" && $3=="freeplay_dj_estado" {printf "%s ", $4}' "$LOG")
DJVOLTA=$(awk -F, '$1=="E" && $3=="freeplay_dj_volta" {print $4","$5; exit}' "$LOG")

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
echo "  DJ (abriu, B por quadro) .... ${DJ:-?}   (370x375 em 16 bpp = 277500)"
echo "  DJ: desenhos ................ ${DJDRW:-?}"
echo "  DJ: estados ................. ${DJEST:-?}   (0=intro 1=idle 2=confirm 5=fist 7=fistLoop)"
echo "  DJ: volta do resultado ...... ${DJVOLTA:-?}   (rank, estado)"
echo "  DJ: quadro que falhou ....... ${DJFALHA:-nenhum}"
echo "  DJ: tamanho recusado ........ ${DJTAM:-nao}"
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
# O DJ: pacote aberto com o tamanho do header (w*h*2), nenhum quadro recusado,
# e pelo menos um desenho -- existir sem desenhar foi o defeito da 1a versao.
[ "${DJ%%,*}" = "1" ] || { echo "  FALHA: o pacote do DJ nao abriu (${DJ:-sem evento})"; FALHA=1; }
[ "${DJ##*,}" = "277500" ] || { echo "  FALHA: o DJ abriu com ${DJ##*,} B por quadro e o header manda 277500"; FALHA=1; }
[ -z "${DJFALHA:-}" ] || { echo "  FALHA: o quadro $DJFALHA do DJ nao descomprimiu"; FALHA=1; }
[ -z "${DJTAM:-}" ] || { echo "  FALHA: o pacote do DJ foi recusado por tamanho ($DJTAM)"; FALHA=1; }
[ "${DJDRW:-0}" -ge 1 ] || { echo "  FALHA: o DJ nunca foi desenhado"; FALHA=1; }
# Idle (1) tem de aparecer (o Intro termina) e o Confirm (2) tambem: sao os dois
# estados que o caminho normal da tela atravessa.
case " ${DJEST:-} " in *" 1 "*) ;; *) echo "  FALHA: o DJ nunca saiu do Intro"; FALHA=1;; esac
case " ${DJEST:-} " in *" 2 "*) ;; *) echo "  FALHA: o DJ nao tocou o Confirm ao escolher a musica"; FALHA=1;; esac
# A VOLTA DO RESULTADO (`FreeplayState.hx` L1084 + L1259, `BaseFreeplayDJ.hx`
# L171-221): o DJ faz `fistPumpIntro()` -- estado 5 (`fistPump`) ou 6 (`loss`,
# quando o rank e' SHIT) -- e 0,5 s depois `fistPump()`/`fistPumpLoss()`, que
# sao os estados 7 e 8. Qual dos dois pares depende do RANK, entao o gate cobra
# o par que corresponde ao rank que a volta registrou:
#   rank 0 (SHIT)          -> 6 e 8   (a reacao de perda, com a animacao `loss`)
#   rank 1..5              -> 5 e 7   (o fist pump, com a animacao `fistPump`)
[ -n "${DJVOLTA:-}" ] || { echo "  FALHA: voltando do resultado o DJ nao recebeu o rank (sem fist pump)"; FALHA=1; }
DJRANK=${DJVOLTA%%,*}
if [ "$DJRANK" = "0" ]; then INI_ESP=6; LOOP_ESP=8; else INI_ESP=5; LOOP_ESP=7; fi
case " ${DJEST:-} " in *" $INI_ESP "*) ;; *) echo "  FALHA: com rank $DJRANK o DJ devia entrar no estado $INI_ESP e nao entrou (${DJEST:-})"; FALHA=1;; esac
case " ${DJEST:-} " in *" $LOOP_ESP "*) ;; *) echo "  FALHA: com rank $DJRANK o DJ devia passar ao estado $LOOP_ESP 0,5 s depois (${DJEST:-})"; FALHA=1;; esac
# E o `fistPump`/`loss` tem de acabar no Idle: `onFinishAnim` L210-214 manda os
# dois para o Idle, e o `loopEndFrame: -1` do JSON quer dizer "sem repetir".
case " ${DJEST:-} " in *" $LOOP_ESP 1 "*) ;; *) echo "  FALHA: o DJ nao voltou ao Idle depois do fist pump (${DJEST:-})"; FALHA=1;; esac
[ -n "${FPLAY:-}" ] || { echo "  FALHA: o CROSS nao tocou nenhuma musica"; FALHA=1; }
[ "${FDIFF:-?}" -ge 1 ] || { echo "  FALHA: a troca de dificuldade nao aconteceu"; FALHA=1; }
[ -n "${CHART:-}" ] && [ "${CHART:-0}" != "0" ] || { echo "  FALHA: o chart da musica escolhida nao carregou"; FALHA=1; }
[ "${NOTAS:-0}" -gt 0 ] || { echo "  FALHA: o total de notas saiu zero"; FALHA=1; }
[ "$AUDIO" -ge 1 ] || { echo "  FALHA: a musica da Freeplay nao comecou a tocar"; FALHA=1; }
[ "$VOLTOU" -ge 2 ] || { echo "  FALHA: depois do resultado o jogo nao voltou para a Freeplay"; FALHA=1; }
# E A CINEMATICA DE RANK NAO PODE APARECER AQUI: o roteiro chega ao resultado
# pelo Practice Mode, e o `ResultState.hx` L938-967 nao manda o `fromResults`
# com `playRankAnim` quando o placar nao e' valido (`isScoreValid`). O caminho
# que FAZ a cinematica e' o `tools/run_rank_anim_harness.sh`; aqui o que se
# cobra e' o portao: sem rank pessoal, a lista responde na hora.
RKANIM=$(awk -F, '$1=="E" && $3=="freeplay_rank_inicio" {n++} END{print n+0}' "$LOG")
RKPED=$(awk -F, '$1=="E" && $3=="freeplay_rank_pedido" {print $4; exit}' "$LOG")
echo "  cinematica de rank ......... pedido=${RKPED:-?} inicios=$RKANIM   (0 = nao roda em practice)"
[ "${RKPED:-0}" = "0" ] || { echo "  FALHA: a cinematica foi pedida em Practice Mode"; FALHA=1; }
[ "$RKANIM" = "0" ] || { echo "  FALHA: a cinematica de rank rodou sem rank pessoal"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "FREEPLAY: FALHOU"
    exit 1
fi

echo
echo "FREEPLAY: PASSOU (lista, dificuldade, chart da musica escolhida e volta)"
