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
# 6800:CIRCLE sai da Freeplay para o menu -- e' o caminho do `goBack`, que toca
# `cancelMenu` (L2240) e so' para o som depois do fade.
ENTRADA="200:CROSS,420:CROSS,640:DOWN,700:CROSS,840:RIGHT,900:CROSS,1200:START,1260:DOWN,1264:DOWN,1268:DOWN,1280:CROSS,1400:START,5750:CIRCLE,6800:CIRCLE"

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
# O AUDIO DA TELA (engine/freeplay_audio.c): o preview do instrumental entra
# 0,25 s depois de a tela abrir (`FADE_IN_DELAY`, FreeplayState.hx L141), toca
# 20 % do arquivo (`Constants.DEFAULT_PREVIEW_*`) em laco e e' reagendado a
# cada troca de musica. O tamanho da janela vem do proprio `stems.pcm`.
PREVN=$(awk -F, '$1=="E" && $3=="freeplay_audio_preview" {n++} END{print n+0}' "$LOG")
PREV=$(awk -F, '$1=="E" && $3=="freeplay_audio_preview" {print $4","$5; exit}' "$LOG")
JANELA=$(awk -F, '$1=="E" && $3=="freeplay_audio_janela" {print $4","$5; exit}' "$LOG")
ATRASO=$(awk -F, '$1=="E" && $3=="freeplay_audio_atraso" {n++} END{print n+0}' "$LOG")
AUDFALHA=$(awk -F, '$1=="E" && ($3=="freeplay_audio_falta" || $3=="freeplay_audio_vazio" || $3=="freeplay_audio_janela_ruim" || $3=="freeplay_audio_ring_falhou" || $3=="freeplay_audio_sem_musica") {printf "%s ", $3}' "$LOG")
# OS `playOnce` DE MENU: `scrollMenu` a 0,4 na troca de musica/dificuldade
# (`FreeplayState.hx` L2918 e L2388) e `confirmMenu` no CROSS (L2764) -- com o
# volume que a fonte manda. O `cancelMenu` do CIRCLE (L2240) so' toca no fim do
# roteiro, quando a tela ja' vai sair.
UISOM=$(awk -F, '$1=="E" && $3=="ui_som" {printf "%s:%s ", $4, $5}' "$LOG")
UISOMFALHA=$(awk -F, '$1=="E" && ($3=="ui_som_falta" || $3=="ui_som_sem_anel") {printf "%s:%s ", $3, $4}' "$LOG")
MENU=$(awk -F, '$1=="S" && $3==6 {n++} END{print n+0}' "$LOG")

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
echo "  audio: previews iniciados ... $PREVN   (amostras, indice da musica: ${PREV:-?})"
echo "  audio: janela em KB ......... ${JANELA:-?}   (0 -> 20 % do instrumental)"
echo "  audio: reagendamentos ....... $ATRASO   (0,25 s depois de abrir e a cada troca de musica)"
echo "  audio: falhas ............... ${AUDFALHA:-nenhuma}"
echo "  sons de menu (indice:volume)  ${UISOM:-nenhum}   (0 scroll 40 / 1 confirm 100 / 2 cancel 100)"
echo "  sons de menu que falharam ... ${UISOMFALHA:-nenhum}"
echo "  vezes no menu (estado 6) .... $MENU"

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
# O AUDIO: pelo menos um preview (a tela abriu duas vezes, entao sao dois), com
# a janela comecando em 0 e terminando em 20 % do `stems.pcm` da musica. E
# nenhuma falha de abertura.
[ "${PREVN:-0}" -ge 2 ] || { echo "  FALHA: o preview da musica tocou ${PREVN:-0} vez(es), menos que 2"; FALHA=1; }
[ -z "${AUDFALHA:-}" ] || { echo "  FALHA: audio da Freeplay: $AUDFALHA"; FALHA=1; }
# OS TRES `playOnce` DE MENU. O scroll tem de ter tocado nas trocas (musica e
# dificuldade) e o confirm no CROSS, com os volumes da fonte (0,4 e 1,0); o
# cancel, na saida.
case " ${UISOM:-} " in *" 0:40 "*) ;; *) echo "  FALHA: o scrollMenu (0,4) nao tocou na troca de selecao (${UISOM:-})"; FALHA=1;; esac
# O `confirmMenu` do TITULO e' o mesmo som a 0,7 (`TitleState.hx` L288): e' a
# primeira coisa que o roteiro faz (200:CROSS).
case " ${UISOM:-} " in *" 1:70 "*) ;; *) echo "  FALHA: o confirmMenu do titulo (0,7) nao tocou (${UISOM:-})"; FALHA=1;; esac
case " ${UISOM:-} " in *" 1:100 "*) ;; *) echo "  FALHA: o confirmMenu nao tocou no CROSS (${UISOM:-})"; FALHA=1;; esac
case " ${UISOM:-} " in *" 2:100 "*) ;; *) echo "  FALHA: o cancelMenu nao tocou na saida da tela (${UISOM:-})"; FALHA=1;; esac
[ -z "${UISOMFALHA:-}" ] || { echo "  FALHA: som de menu nao abriu/sem anel: $UISOMFALHA"; FALHA=1; }
# E a saida tem de ter chegado no menu (estado 6) sem matar o som do estado novo.
[ "${MENU:-0}" -ge 1 ] || { echo "  FALHA: o CIRCLE no fim nao voltou para o menu"; FALHA=1; }
if [ -z "${JANELA:-}" ]; then
    echo "  FALHA: a janela do preview nunca foi calculada"; FALHA=1
else
    JINI=${JANELA%%,*}; JFIM=${JANELA##*,}
    [ "$JINI" = "0" ] || { echo "  FALHA: a janela comeca em ${JINI} KB e o padrao e' 0"; FALHA=1; }
    STEMS=build/harness_assets/FNFPS2/assets/songs/tutorial/stems.pcm
    if [ -f "$STEMS" ]; then
        ESP=$(( $(wc -c < "$STEMS") / 5 / 1024 ))
        [ "$JFIM" = "$ESP" ] || { echo "  FALHA: a janela termina em ${JFIM} KB e 20 % do stems da' $ESP KB"; FALHA=1; }
    fi
fi

if [ "$FALHA" != 0 ]; then
    echo
    echo "FREEPLAY: FALHOU"
    exit 1
fi

# ---- 3. O BRILHO DO SELO PERFECT_GOLD ------------------------------------
# O `sparkle` so' existe quando o rank e' PERFECT_GOLD (`updateScoringRank`,
# `SongMenuItem.hx` L493-504). O caminho normal deste harness toca a musica no
# Practice Mode, entao o rank salvo nunca e' dourado -- sem um save dourado o
# brilho nao teria prova nenhuma. Aqui se escreve um `save.dat` com UM registro
# valido e 100 % `sick` (`fnf_play_rank`: sick == total_notes -> PERFECT_GOLD)
# e se cobra: o pacote abriu com os 1.300 B do header, o brilho foi DESENHADO e
# o primeiro desenho foi o quadro 0 (o disparo de 1 s), na posicao sorteada.
echo
echo "== freeplay: o brilho do selo PERFECT_GOLD (save com rank 5) =="
SAVE=build/harness_assets/FNFPS2/save.dat
mkdir -p build/harness_assets/FNFPS2
python3 - "$SAVE" <<'PY2'
import struct, sys, zlib
b = bytearray(1992)          # versao 2: 16 + 81*24 + 32 (favorito)
b[0:4] = b"FNFS"
struct.pack_into("<HHI", b, 4, 2, 81, 0)   # versao 2 -- a que tem 1992 B
# musica 0, dificuldade 1 (NORMAL) -- a que a Freeplay abre selecionada.
o = 16 + (0 * 3 + 1) * 24
tot = 48
struct.pack_into("<I", b, o, 350000)
# sick, good, bad, shit, missed, combo, maxCombo, totalHit, totalNotes
struct.pack_into("<9H", b, o + 4, tot, 0, 0, 0, 0, tot, tot, tot, tot)
b[o + 22] = 1                      # valido
struct.pack_into("<I", b, 12, zlib.crc32(bytes(b[16:])) & 0xFFFFFFFF)
open(sys.argv[1], "wb").write(bytes(b))
print("  save dourado escrito: 1.992 B, musica 0 / dificuldade 1, %d/%d sick" % (tot, tot))
PY2
# TRES SQUAREs e o `FREEPLAY_FAVORITE` (`favoriteSong`, FreeplayState.hx
# L3079-3130): 900 marca (som `fav`), 1000 desmarca (som `unfav` e a animacao ao
# REVERSO, `play('fav', true, true, 9)`) e 1100 marca de novo -- assim os dois
# sentidos rodam e o arquivo que sobra no cartao termina favoritado. A Freeplay
# abre no quadro 796 deste roteiro (medido) e a transicao leva ~15 quadros,
# entao 900 e' o primeiro SQUARE que a lista ja' responde. O original nao liga
# botao nenhum nesse controle (so' o teclado, tecla F): o SQUARE e' escolha do
# port, ver docs/PLANO_COMPLETO.md.
ENTRADA_OURO="200:CROSS,420:CROSS,640:DOWN,700:CROSS,900:SQUARE,1000:SQUARE,1100:SQUARE"
rm -f "$LOG"
FNF_CARD=1 PS2H_INPUT="$ENTRADA_OURO" sh tools/run_harness.sh 1200 > /tmp/freeplay_ouro.log 2>&1 || {
    echo "  FALHA: o harness nao passou com o save dourado"; tail -20 /tmp/freeplay_ouro.log; exit 1; }
BRILHO=$(awk -F, '$1=="E" && $3=="freeplay_brilho" {print $4","$5; exit}' "$LOG")
BRILHOFALHA=$(awk -F, '$1=="E" && ($3=="freeplay_brilho_falta" || $3=="freeplay_brilho_tamanho" || $3=="freeplay_brilho_sem_vram") {printf "%s ", $3}' "$LOG")
OURO=$(awk -F, '$1=="E" && $3=="freeplay_selo_ranks" {print $4; exit}' "$LOG")
NDESOURO=$(awk -F, '$1=="E" && $3=="freeplay_brilho_desenho" {n++} END{print n+0}' "$LOG")
QOURO=$(awk -F, '$1=="E" && $3=="freeplay_brilho_desenho" {print $4; exit}' "$LOG")
XOURO=$(awk -F, '$1=="E" && $3=="freeplay_brilho_desenho" {print $5; exit}' "$LOG")
# O DISPARO: o quadro 0 so' existe quando o `sparkleEffect` roda. O x e o y
# sao o sorteio da fonte (x -20..+3, y -29..+4) em decimos de px.
DISPARO=$(awk -F, '$1=="E" && $3=="freeplay_brilho_disparo" {print $4","$5; exit}' "$LOG")
echo "  brilho (abriu, B) ........... ${BRILHO:-?}   (26x25 em 16 bpp = 1300 B)"
echo "  ranks dourados na lista ..... ${OURO:-?}"
echo "  brilho desenhado ............ quadro ${QOURO:-?} em ${NDESOURO:-0} desenho(s), x=${XOURO:-?} decimos"
echo "  brilho disparou ............. ${DISPARO:-?}   (x,y em decimos: 2000..2115 e 60..225)"
FVPAC=$(awk -F, '$1=="E" && $3=="freeplay_fav" {print $4","$5; exit}' "$LOG")
FVDES=$(awk -F, '$1=="E" && $3=="freeplay_fav_desenho" {print $4","$5; exit}' "$LOG")
FVMAR=$(awk -F, '$1=="E" && $3=="freeplay_fav_marca" {print $4","$5; exit}' "$LOG")
FVSO=$(awk -F, '$1=="E" && $3=="ui_som" && $4==3 {print $4":"$5; exit}' "$LOG")
FVSO4=$(awk -F, '$1=="E" && $3=="ui_som" && $4==4 {print $4":"$5; exit}' "$LOG")
FVMARKS=$(awk -F, '$1=="E" && $3=="freeplay_fav_marca" {printf "%s", $4}' "$LOG")
echo "  coracao (abriu, B) .......... ${FVPAC:-?}   (25x25 em 16 bpp = 1250 B)"
echo "  coracao desenhado ........... ${FVDES:-?}   (quadro,x em decimos)"
echo "  coracao marcou .............. ${FVMAR:-?}   (1=favoritou, indice da musica)"
echo "  coracao som ................. ${FVSO:-?} / ${FVSO4:-?}   (slot 3 = fav, 4 = unfav, 0..100)"
echo "  coracao marcas .............. ${FVMARKS:-?}   (1,0,1: marca, desmarca, marca)"
[ "${BRILHO%%,*}" = "1" ] || { echo "  FALHA: o pacote do brilho nao abriu (${BRILHO:-sem evento})"; FALHA=1; }
[ "${BRILHO##*,}" = "1300" ] || { echo "  FALHA: o brilho abriu com ${BRILHO##*,} B e o header manda 1300"; FALHA=1; }
[ -z "${BRILHOFALHA:-}" ] || { echo "  FALHA: brilho do selo: $BRILHOFALHA"; FALHA=1; }
[ "${OURO:-0}" -ge 1 ] || { echo "  FALHA: o rank dourado do save nao chegou na lista (${OURO:-sem evento})"; FALHA=1; }
[ "${NDESOURO:-0}" -ge 1 ] || { echo "  FALHA: o brilho do selo dourado nunca foi desenhado"; FALHA=1; }
[ "${QOURO:-?}" = "9" ] || { echo "  FALHA: o primeiro brilho saiu no quadro ${QOURO} e o sprite comeca no ULTIMO (9, o 1x1 vazio)"; FALHA=1; }
[ "${FVPAC%%,*}" = "1" ] || { echo "  FALHA: o pacote do coracao nao abriu (${FVPAC:-sem evento})"; FALHA=1; }
[ "${FVPAC##*,}" = "1250" ] || { echo "  FALHA: o coracao abriu com ${FVPAC##*,} B e o header manda 1250"; FALHA=1; }
[ "${FVMAR%%,*}" = "1" ] || { echo "  FALHA: o SQUARE nao marcou a musica como favorita (${FVMAR:-sem evento})"; FALHA=1; }
[ -n "${FVDES:-}" ] || { echo "  FALHA: o coracao nunca foi desenhado"; FALHA=1; }
[ "${FVSO:-}" = "3:100" ] || { echo "  FALHA: o som de favoritar nao tocou a 1,0 (${FVSO:-nenhum})"; FALHA=1; }
[ "${FVSO4:-}" = "4:100" ] || { echo "  FALHA: o som de desfavoritar nao tocou a 1,0 (${FVSO4:-nenhum})"; FALHA=1; }
[ "${FVMARKS:-}" = "101" ] || { echo "  FALHA: as marcas do coracao foram '${FVMARKS:-}' e nao 1,0,1"; FALHA=1; }
[ -n "${DISPARO:-}" ] || { echo "  FALHA: o brilho nunca disparou (o timer de 1 s nao chegou 'a tela)"; FALHA=1; }
if [ -n "${DISPARO:-}" ]; then
    DY=${DISPARO%%,*}; DYY=${DISPARO##*,}
    [ "$DY" -ge 2000 ] && [ "$DY" -le 2115 ] || {
        echo "  FALHA: o disparo saiu em x=$DY decimos e o sorteio e' 2000..2115"; FALHA=1; }
    [ "$DYY" -ge 60 ] && [ "$DYY" -le 225 ] || {
        echo "  FALHA: o disparo saiu em y=$DYY decimos e o sorteio e' 60..225"; FALHA=1; }
fi
# A posicao sorteada: `ranking.x - 20..+3` e `ranking.y - 29..+4`, em 640x360.
# O `x` do ranking e' 210,0 -> o desenho tem de cair em 200,0..211,5 (2000..2115
# em decimos). Fora disso o sorteio esta' no espaco errado.
if [ -n "${XOURO:-}" ]; then
    [ "$XOURO" -ge 2000 ] && [ "$XOURO" -le 2115 ] || {
        echo "  FALHA: o brilho saiu em x=$XOURO decimos e o sorteio e' 2000..2115"; FALHA=1; }
fi

# O SAVE NO "CARTAO": o `favoriteSong()` passa por `toggleFavorite()` ->
# `Save.save()` (`FreeplayState.hx` L3130), entao o arquivo reescrito tem de ter
# o byte da musica 0 em 1. O favorito e' um byte por musica logo depois dos 81
# registros de 24 B: corpo = 16 do cabecalho + 1944 + indice; total 1992 B.
if [ -f "$SAVE" ]; then
    FBYTE=$(python3 - "$SAVE" <<'PY2'
import sys
b = open(sys.argv[1], "rb").read()
o = 16 + 81 * 24
print("%d,%d" % (len(b), b[o] if len(b) > o else -1))
PY2
)
    echo "  coracao no save ............. ${FBYTE:-?}   (bytes, byte dos favoritos)"
    [ "${FBYTE%%,*}" = "1992" ] || { echo "  FALHA: o save no cartao ficou com ${FBYTE%%,*} B e o formato manda 1992"; FALHA=1; }
    [ "${FBYTE##*,}" = "1" ] || { echo "  FALHA: o save gravado nao tem o favorito da musica 0 (byte ${FBYTE##*,})"; FALHA=1; }
fi

if [ "$FALHA" != 0 ]; then
    echo
    echo "FREEPLAY: FALHOU"
    exit 1
fi

echo
echo "FREEPLAY: PASSOU (lista, dificuldade, chart, brilho, coracao e volta)"
