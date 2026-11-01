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
#   2) com CROSS 120 quadros depois da morte, para provar que o reinicio volta a
#      tocar a musica.
# O roteiro e' deterministico, entao as duas passadas sao comparaveis.
#
# DESDE QUE A TELA DE GAME OVER TEM A ANIMACAO E A CAMERA DO ORIGINAL, o que se
# cobra aqui e' mais que "a tela existe":
#   - `gameover_primeira_morte`: o BF entrou no `firstDeath` (e nao no idle);
#   - `gameover_loop_morte`: quando o `firstDeath` acabou, entrou o `deathLoop`
#     e o personagem NAO voltou a dancar (cc_especial_fixo);
#   - `gameover_confirmar` = 1: o CROSS achou o `deathConfirm`;
#   - o reinicio acontece FADE_TIMER + 120 quadros DEPOIS do CROSS: o
#     confirmDeath espera `gameOverEnd.length / 7000` e entao faz
#     FlxG.camera.fade(BLACK, 2) -- 2 s indo para preto (GameOverSubState.hx
#     L369-438). O teste le' o FADE_TIMER que o proprio jogo publicou
#     (`gameover_fade_espera`) em vez de fixar um numero, porque ele depende do
#     comprimento do arquivo;
#   - O AUDIO DA TELA EXISTE: `fnf_loss_sfx`, a musica `gameOver` em loop e o
#     `gameOverEnd` do confirmDeath (os tres eram dado ausente);
#   - a camera terminou no ponto que o setCameraTarget manda e o zoom no
#     `stage.camZoom * death.cameraZoom`.
# O CROSS e' aceito no quadro seguinte a morte: no original o `canInput` de 1 s
# so' vale para o toque, o ACCEPT_P do controle nao tem bloqueio
# (GameOverSubState.hx L271-278). Antes o port bloqueava 60 quadros por engano.
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

# Passada 3: CIRCLE (BACK) -> goBack(): sai para o menu e zera o "BLUE BALLS"
# (`parentPlayState.deathCounter = 0`, GameOverSubState.hx L548).
rm -f "$LOG"
echo "== game over: passada 3 (CIRCLE 120 quadros depois da morte) =="
PS2H_INPUT="$ENTRADA,${RETRY}:CIRCLE" sh tools/run_harness.sh "$FRAMES" >/dev/null 2>&1 || {
    echo "  FALHA: o harness nao terminou a passada 3"; exit 1; }
GEXIT=$(awk -F, '$1=="E" && $3=="gameover_exit" {print $4; exit}' "$LOG")
GRESTART3=$(awk -F, '$1=="E" && $3=="gameover_restart" {n++} END{print n+0}' "$LOG")
echo "  saida pelo CIRCLE em ........ ${GEXIT:-?} (reinicios: $GRESTART3)"
[ -n "$GEXIT" ] || { echo "  FALHA: o CIRCLE nao saiu da tela de game over"; exit 1; }
[ "$GRESTART3" -eq 0 ] || { echo "  FALHA: o CIRCLE reiniciou a musica (so' o CROSS pode)"; exit 1; }

# A passada 2 e' a que tem os numeros da tela: roda de novo (com o log limpo,
# senao os contadores somam os da passada 3) para os AWKs abaixo.
rm -f "$LOG"
PS2H_INPUT="$ENTRADA,${RETRY}:CROSS" sh tools/run_harness.sh "$FRAMES" >/dev/null 2>&1 || {
    echo "  FALHA: o harness nao terminou a passada 2 (repeticao)"; exit 1; }

PRIMEIRA=$(awk -F, '$1=="E" && $3=="gameover_primeira_morte" {print $4; exit}' "$LOG")
LOOP=$(awk -F, '$1=="E" && $3=="gameover_loop_morte" {print $5; exit}' "$LOG")
CONFIRMA=$(awk -F, '$1=="E" && $3=="gameover_confirmar" {print $4; exit}' "$LOG")
CAMX=$(awk -F, '$1=="E" && $3=="gameover_cam_final" {print $4; exit}' "$LOG")
CAMY=$(awk -F, '$1=="E" && $3=="gameover_cam_final" {print $5; exit}' "$LOG")
ZOOM=$(awk -F, '$1=="E" && $3=="gameover_zoom_final" {print $4; exit}' "$LOG")
ZOOMALVO=$(awk -F, '$1=="E" && $3=="gameover_zoom_final" {print $5; exit}' "$LOG")
# O FOCO nao e' constante: ele depende do canvas do pacote, e no harness o
# canvas e' SINTETICO (a arte nao esta' no repositorio). O jogo publica o alvo
# que calculou (`gameover_foco`, do ch_foco_morte) e o teste cobra que a camera
# CHEGOU nele. A formula em si -- com o canvas do pacote REAL, 730x556 -- esta'
# presa em test/test_char_layout.c: (1033,5 ; 763,0) para o BF do mainStage.
FOCO_X=$(awk -F, '$1=="E" && $3=="gameover_foco" {print $4; exit}' "$LOG")
FOCO_Y=$(awk -F, '$1=="E" && $3=="gameover_foco" {print $5; exit}' "$LOG")

DEATH=$(awk -F, '$1=="E" && $3=="play_death" {print $5; exit}' "$LOG")
# AUDIO: sfx do blueball, musica em loop e o End do confirmDeath.
GASFX=$(awk -F, '$1=="E" && $3=="gameover_audio_inicio" {print $4; exit}' "$LOG")
GMUS=$(awk -F, '$1=="E" && $3=="gameover_musica" {print $4; exit}' "$LOG")
GEND=$(awk -F, '$1=="E" && $3=="gameover_end" {print $4; exit}' "$LOG")
GFADE=$(awk -F, '$1=="E" && $3=="gameover_fade_espera" {print $4; exit}' "$LOG")
GSUB=$(awk -F, '$1=="E" && $3=="gameover_audio_fim" {print $4; exit}' "$LOG")
GSEMUS=$(awk -F, '$3=="gameover_sem_musica" {n++} END{print n+0}' "$LOG")
GSAUS=$(awk -F, '$3=="gameover_sfx_ausente" {n++} END{print n+0}' "$LOG")
GOLDSEM=$(awk -F, '$3=="gameover_sem_audio" {n++} END{print n+0}' "$LOG")
GENTER=$(awk -F, '$1=="E" && $3=="gameover_enter" {print $4; exit}' "$LOG")
GDRAW=$(awk -F, '$1=="E" && $3=="gameover_draw" {print $4; exit}' "$LOG")
# O QUE O `gameover_bf` PUBLICA. Campo 4 = bits das quatro condicoes (1 = cena
# generica, 2 = papel ativo, 4 = textura com largura, 8 = slot `bf` do palco
# achado); campo 5 = largura do canvas do pacote que o game over esta'
# desenhando.
#
# A largura NAO e' um numero fixo: e' a do pacote CHP4 que a musica carregou.
# No harness os pacotes sao SINTETICOS (64x64 -- a arte nao esta' no
# repositorio), entao 64 aqui e' o esperado; com os pacotes de verdade o BF do
# mainStage tem 365x278 (`build/packs/chars/bf.chp4`). O que o teste cobra,
# entao, nao e' o numero: e' que a tela de game over desenhe o MESMO pacote que
# a musica usou -- o jogo publica a largura nos dois momentos
# (`char_bf_primeiro` no gameplay, `gameover_bf` na tela), e as duas tem de
# bater. Antes disso, "o BF apareceu pequeno" nao tinha como ser distinguido de
# "o BF esta' sendo desenhado de outro pacote".
GBITS=$(awk -F, '$1=="E" && $3=="gameover_bf" {print $4; exit}' "$LOG")
GBW=$(awk -F, '$1=="E" && $3=="gameover_bf" {print $5; exit}' "$LOG")
BFPACK=$(awk -F, '$1=="E" && $3=="char_bf_primeiro" {print $5; exit}' "$LOG")
# `GameOverSubState.create()` nao poe TEXTO nenhum na tela de game over (so' o
# retangulo preto, o BF morto e o ponto de camera). O campo 5 do evento conta os
# quads de texto desenhados ali, e tem de ser zero: o port ja' desenhou
# "GAME OVER"/"MISSES"/"CROSS RETRY CIRCLE EXIT", que o original nunca mostra.
GTXT=$(awk -F, '$1=="E" && $3=="gameover_draw" {print $5; exit}' "$LOG")
GOUT=$(awk -F, '$1=="E" && $3=="gameover_restart" {print $4; exit}' "$LOG")
GSTATE=$(awk -F, '$1=="S" && $3==10 {n++} END{print n+0}' "$LOG")
CINCOS=$(awk -F, '$1=="E" && $3=="countdown_step" && $4==5 {n++} END{print n+0}' "$LOG")
AUDIOS=$(awk -F, '$1=="E" && $3=="tutorial_audio_start" {n++} END{print n+0}' "$LOG")
MISSES=$(awk -F, '$1=="E" && $3=="play_death" {print $4; exit}' "$LOG")

echo
echo "  quadro da morte ............. ${DEATH:-?}"
echo "  tela de game over desde ..... ${GENTER:-?}"
echo "  quadros em ui_state 10 ...... $GSTATE"
echo "  quads desenhados na tela 10 . ${GDRAW:-?} (texto: ${GTXT:-?})"
echo "  BF do game over ............. bits $GBITS, canvas ${GBW:-?} px (musica: ${BFPACK:-?})"
echo "  misses na morte ............. ${MISSES:-?}"
echo "  firstDeath (personagem) ..... ${PRIMEIRA:-?}"
echo "  deathLoop no quadro ......... ${LOOP:-?}"
echo "  deathConfirm achada ......... ${CONFIRMA:-?}"
echo "  camera final ................ ${CAMX:-?},${CAMY:-?} (foco ${FOCO_X:-?},${FOCO_Y:-?})"
echo "  zoom final / alvo ........... ${ZOOM:-?} / ${ZOOMALVO:-?} (milesimos)"
echo "  reinicio em ................. ${GOUT:-?}"
echo "  sfx do blueball aberto ...... ${GASFX:-?}"
echo "  musica de game over (amostras) ${GMUS:-?}"
echo "  gameOverEnd (amostras) ...... ${GEND:-?}"
echo "  FADE_TIMER (ms) ............. ${GFADE:-?}"
echo "  amostras tocadas na tela .... ${GSUB:-?}"
echo "  vezes que a musica comecou .. $AUDIOS"
echo "  contagens terminadas ........ $CINCOS"

FALHA=0
[ -n "$DEATH" ]  || { echo "  FALHA: nenhum play_death no log"; FALHA=1; }
[ -n "$GENTER" ] || { echo "  FALHA: a tela de game over nunca abriu"; FALHA=1; }
[ "$GSTATE" -ge 30 ] || { echo "  FALHA: ui_state 10 durou menos de 30 quadros"; FALHA=1; }
[ "${GDRAW:-0}" -gt 0 ] || { echo "  FALHA: a tela de game over nao desenhou nada"; FALHA=1; }
[ "${GTXT:-1}" -eq 0 ] || { echo "  FALHA: a tela de game over desenhou texto -- o original nao tem nenhum"; FALHA=1; }
[ -n "$GOUT" ]   || { echo "  FALHA: o CROSS nao reiniciou a musica"; FALHA=1; }
# O BF MORTO: as quatro condicoes ligadas e o MESMO canvas da musica.
[ "${GBITS:-0}" -eq 15 ] || { echo "  FALHA: o BF morto nao chegou ao desenho (gameover_bf bits=$GBITS, esperado 15)"; FALHA=1; }
[ -n "${GBW:-}" ] && [ -n "${BFPACK:-}" ] || { echo "  FALHA: o jogo nao publicou a largura do canvas do BF"; FALHA=1; }
[ "${GBW:-0}" -gt 0 ] || { echo "  FALHA: o canvas do BF no game over tem largura zero"; FALHA=1; }
if [ -n "${GBW:-}" ] && [ -n "${BFPACK:-}" ] && [ "$GBW" != "$BFPACK" ]; then
    echo "  FALHA: o game over desenha um pacote de $GBW px, mas a musica carregou $BFPACK px"; FALHA=1; fi
[ "$AUDIOS" -ge 2 ] || { echo "  FALHA: a musica nao voltou a tocar depois do reinicio"; FALHA=1; }
[ "$CINCOS" -ge 2 ] || { echo "  FALHA: a contagem regressiva nao recomecou"; FALHA=1; }
# Uma morte sem nota perdida nao faz sentido: a vida so' cai por miss/ghost.
[ "${MISSES:-0}" -ge 13 ] || { echo "  FALHA: morte com menos de 13 misses (a vida nao caiu como devia)"; FALHA=1; }
# O BECO SEM SAIDA DE 1 EM 4096. Se o sorteio cair no `fakeoutDeath`, o
# original NAO marca `blueballed` e a tela nao sai nem com CROSS nem com CIRCLE
# (GameOverSubState.hx L250-262 + L271-278). O port reproduz isso; o teste avisa
# em vez de acusar "o CROSS nao reiniciou" sem explicar.
if awk -F, '$3=="gameover_fakeout_sem_saida" {f=1} END{exit !f}' "$LOG"; then
    echo "  AVISO: esta rodada caiu no fakeoutDeath (1/4096) -- o original tambem"
    echo "         nao sai dessa tela por CROSS nem por CIRCLE. Rode de novo."
    echo "GAME OVER: INCONCLUSIVO (sorteio do fakeout)"
    exit 2
fi

# A ANIMACAO e a CAMERA do substate.
[ -n "$FOCO_X" ] || { echo "  FALHA: o jogo nao publicou o foco da camera (gameover_foco)"; FALHA=1; }
[ -n "$PRIMEIRA" ] || { echo "  FALHA: o BF nao entrou no firstDeath (gameover_primeira_morte)"; FALHA=1; }
[ -n "$LOOP" ] || { echo "  FALHA: o deathLoop nunca entrou depois do firstDeath"; FALHA=1; }
if [ -n "$LOOP" ] && [ -n "$GENTER" ] && [ "$LOOP" -le "$GENTER" ]; then
    echo "  FALHA: deathLoop ($LOOP) antes da tela abrir ($GENTER)"; FALHA=1; fi
case "${CONFIRMA:-}" in 1) ;; *) echo "  FALHA: o CROSS nao achou o deathConfirm"; FALHA=1;; esac
# SAIDA: FADE_TIMER (do comprimento do gameOverEnd) + 2 s de fade para preto.
# O FADE_TIMER vem do proprio jogo, entao o teste nao fixa o numero: ele cobra a
# CONTA. 60 quadros = 1 s; 120 = 2 s. Tolerancia de 3 quadros (arredondamento do
# `>=` do relogio e o quadro do CROSS).
if [ -n "$GOUT" ]; then
    DIF=$((GOUT - RETRY))
    if [ -n "${GFADE:-}" ]; then EQ=$(( (GFADE * 60 + 999) / 1000 )); else EQ=0; fi
    ESP=$((EQ + 120))
    echo "  fade do confirmDeath ........ $DIF quadros (esperado $ESP = ${GFADE:-?}ms + 120)"
    if [ "$DIF" -lt $((ESP - 3)) ] || [ "$DIF" -gt $((ESP + 3)) ]; then
        echo "  FALHA: o reinicio nao esperou FADE_TIMER + fade de 2 s ($DIF quadros)"; FALHA=1; fi
fi
# A camera converge para o foco (60 quadros de fade quase fecham a conta:
# 1 - 0,98^180 = 0,973 do caminho; 40 px de tolerancia cobre o resto).
if [ -n "$CAMX" ] && [ -n "$CAMY" ] && [ -n "$FOCO_X" ] && [ "${FOCO_X:--1}" -ge 0 ]; then
    DX=$((CAMX - FOCO_X)); DY=$((CAMY - FOCO_Y))
    [ "$DX" -lt 0 ] && DX=$((-DX)); [ "$DY" -lt 0 ] && DY=$((-DY))
    if [ "$DX" -gt 40 ] || [ "$DY" -gt 40 ]; then
        echo "  FALHA: a camera parou longe do foco do BF (${DX}px, ${DY}px)"; FALHA=1; fi
fi
[ "${ZOOM:-0}" -eq "${ZOOMALVO:-1}" ] || { echo "  FALHA: o zoom nao chegou em stage.camZoom * death.cameraZoom"; FALHA=1; }

# AUDIO DA TELA. O `gameover_sem_audio` era o evento que declarava "esta tela nao
# tem som" -- ele nao pode mais existir, e os tres sons tem de ter aberto.
[ "${GASFX:-0}" -eq 1 ] || { echo "  FALHA: o fnf_loss_sfx nao abriu (gameover_audio_inicio != 1)"; FALHA=1; }
[ "${GSEMUS:-0}" -eq 0 ] || { echo "  FALHA: a musica de game over nao foi encontrada"; FALHA=1; }
[ "${GSAUS:-0}" -eq 0 ] || { echo "  FALHA: o sfx do blueball foi declarado ausente"; FALHA=1; }
[ "${GOLDSEM:-0}" -eq 0 ] || { echo "  FALHA: ainda existe gameover_sem_audio no log"; FALHA=1; }
[ "${GMUS:-0}" -gt 0 ] || { echo "  FALHA: a musica de game over nao abriu (gameover_musica)"; FALHA=1; }
[ "${GEND:-0}" -gt 0 ] || { echo "  FALHA: o gameOverEnd do confirmDeath nao abriu"; FALHA=1; }
[ -n "${GFADE:-}" ] || { echo "  FALHA: o FADE_TIMER nao foi publicado"; FALHA=1; }
[ "${GSUB:-0}" -gt 48000 ] || { echo "  FALHA: quase nada foi enviado ao audsrv na tela de game over (${GSUB:-0} amostras)"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "GAME OVER: FALHOU"
    exit 1
fi

echo
echo "GAME OVER: PASSOU (morte com firstDeath/deathLoop, camera e zoom do original, audio de verdade, FADE_TIMER + 2 s de fade, reinicio tocando de novo)"
