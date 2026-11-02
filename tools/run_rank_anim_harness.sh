#!/bin/sh
# Harness da CINEMATICA DE RANK da Freeplay (a volta do resultado com rank novo).
#
# O que este teste cobra, e por que so' um harness pode cobrar:
#   - a tela de Freeplay, ao voltar do resultado, roda `rankAnimStart` ->
#     `rankDisplayNew` -> `rankAnimSlam` (FreeplayState.hx L1069-1344) em vez de
#     entrar direto na lista;
#   - as TRES texturas da cinematica abriram (`freeplay_cine` = 7, os tres bits)
#     e somam 186.566 B;
#   - as cinco fases aparecem NA ORDEM e com o zoom que a fonte manda em cada
#     uma (1,85 no comeco; 1,5 em 0,9 s; 0,8 no pouso);
#   - a capsula alvo passa pelo CENTRO da tela (167,0 , 147,0), leva o tranco de
#     `x -= 10 / y -= 20` em 0,6 s (157,0 , 127,0) e volta para `originalPos`
#     (160,244 , 117,8) no pouso;
#   - a cinematica ACABA sozinha em 2,2 s (`freeplay_rank_fim`), sem travar a
#     tela -- e ela nao aparece quando o rank nao subiu (o harness da Freeplay
#     roda o caminho do rank 0, que nao faz o slam, e cobra justamente isso).
#
# O Practice Mode e' o caminho do roteiro (sem notas o jogo morre no meio da
# musica), e o original NAO faz a cinematica em practice (`isScoreValid`,
# ResultState.hx L938). `FNF_RANK_ANIM=1` e' a porta do binario de teste; o
# `FNF_RANK=3` forca o rank da tela e o `FNF_RANK_ANTIGO=1` forca o rank que o
# save tinha, para o `fakeRanking` e as faiscas coloridas entrarem.
#
# Uso: FNF_RANK=3 tools/run_rank_anim_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-7000}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt
RANK=${FNF_RANK:-3}
ANTIGO=${FNF_RANK_ANTIGO:-1}

# O MESMO roteiro do harness da Freeplay: menu -> Freeplay -> musica -> pausa ->
# Practice -> resultado -> CIRCLE (a volta que dispara a cinematica).
ENTRADA="200:CROSS,420:CROSS,640:DOWN,700:CROSS,840:RIGHT,900:CROSS,1200:START,1260:DOWN,1264:DOWN,1268:DOWN,1280:CROSS,1400:START,5750:CIRCLE"

rm -f "$LOG"
echo "== cinematica de rank: a volta do resultado (rank $RANK, antigo $ANTIGO)"
FNF_RANK=$RANK FNF_RANK_ANIM=1 FNF_RANK_ANTIGO=$ANTIGO \
    PS2H_INPUT="$ENTRADA" sh tools/run_harness.sh "$FRAMES" || exit 1

CINE=$(awk -F, '$1=="E" && $3=="freeplay_cine" {print $4","$5; exit}' "$LOG")
PED=$(awk -F, '$1=="E" && $3=="freeplay_rank_pedido" {print $4","$5; exit}' "$LOG")
INI=$(awk -F, '$1=="E" && $3=="freeplay_rank_inicio" {print $4","$5; exit}' "$LOG")
FASES=$(awk -F, '$1=="E" && $3=="freeplay_rank_fase" {printf "%s ", $4}' "$LOG")
ZOOMS=$(awk -F, '$1=="E" && $3=="freeplay_rank_fase" {printf "%s ", $5}' "$LOG")
POS=$(awk -F, '$1=="E" && $3=="freeplay_rank_pos" {printf "%s:%s ", $4, $5}' "$LOG")
FIM=$(awk -F, '$1=="E" && $3=="freeplay_rank_fim" {print $4","$5; exit}' "$LOG")
DES=$(awk -F, '$1=="E" && $3=="freeplay_rank_desenho" {print $4","$5; exit}' "$LOG")
NDRAW=$(awk -F, '$1=="E" && $3=="freeplay_cine_quadro_falhou" {n++} END{print n+0}' "$LOG")

echo
echo "  texturas da cinematica ..... ${CINE:-?}   (7 = sparks+sparksadd+vinheta)"
echo "  pedido (subiu o rank) ...... ${PED:-?}   (1 = sim; antigo*10+novo)"
echo "  inicio (novo, antigo) ...... ${INI:-?}"
echo "  fases ...................... ${FASES:-?}   (0..4, em ordem)"
echo "  zoom da rankCamera ......... ${ZOOMS:-?}   (x1000)"
echo "  capsula (x:y em decimos) ... ${POS:-?}"
echo "  fim (desenhos, atraso ms) .. ${FIM:-?}"
echo "  desenho (novo, antigo) ..... ${DES:-?}"
echo "  quadros da cinematica que falharam .. $NDRAW"

FALHA=0
[ "${CINE%%,*}" = "7" ] || { echo "  FALHA: as texturas da cinematica nao abriram todas (${CINE:-sem evento})"; FALHA=1; }
[ "${CINE##*,}" = "186566" ] || { echo "  FALHA: a cinematica pede ${CINE##*,} B e o header manda 186566"; FALHA=1; }
[ "${PED%%,*}" = "1" ] || { echo "  FALHA: o resultado nao pediu a cinematica (${PED:-sem evento})"; FALHA=1; }
[ -n "${INI:-}" ] || { echo "  FALHA: a cinematica nunca comecou"; FALHA=1; }
# As cinco fases, na ordem. A 5 nao entra: ela e' "acabou", e o fim e' o
# `freeplay_rank_fim`.
[ "${FASES:-}" = "0 1 2 3 4 " ] || { echo "  FALHA: as fases sairam '${FASES:-}' e deviam ser '0 1 2 3 4'"; FALHA=1; }
# Os zooms de cada fase: no comeco 1,85; em 0,9 s (fase 2) exatamente 1,5; no
# pouso (fase 4) exatamente 0,8.
set -- ${ZOOMS:-}
[ "${1:-}" = "1850" ] || { echo "  FALHA: o zoom no comeco saiu ${1:-?} e era 1850"; FALHA=1; }
[ "${3:-}" = "1500" ] || { echo "  FALHA: o zoom em 0,9 s saiu ${3:-?} e era 1500"; FALHA=1; }
[ "${5:-}" = "800" ] || { echo "  FALHA: o zoom no pouso saiu ${5:-?} e era 800"; FALHA=1; }
# As posicoes: centro (1670:1470), tranco (1570:1270), e o pouso em
# originalPos (1602:1178). A fase 2 (0,9 s) fica entre as duas primeiras.
case " ${POS:-} " in
    *" 1670:1470 "*) ;;
    *) echo "  FALHA: a capsula nao comecou no centro (${POS:-})"; FALHA=1;;
esac
case " ${POS:-} " in
    *" 1570:1270 "*) ;;
    *) echo "  FALHA: a capsula nao levou o tranco de -10/-20 (${POS:-})"; FALHA=1;;
esac
case " ${POS:-} " in
    *" 1602:1178 "*) ;;
    *) echo "  FALHA: a capsula nao pousou em originalPos (${POS:-})"; FALHA=1;;
esac
[ -n "${FIM:-}" ] || { echo "  FALHA: a cinematica nunca terminou"; FALHA=1; }
# Ela tem de acabar em 2,2 s e ter desenhado (existir sem desenhar ja' foi
# defeito nesta tela).
[ "${FIM##*,}" = "0" ] || { echo "  FALHA: a cinematica passou de 2,2 s em ${FIM##*,} ms"; FALHA=1; }
[ "${FIM%%,*}" -ge 20 ] 2>/dev/null || { echo "  FALHA: a cinematica desenhou ${FIM%%,*} elementos, menos que 20"; FALHA=1; }
[ "${DES%%,*}" = "$RANK" ] || { echo "  FALHA: o desenho registrou o rank ${DES%%,*} e o forcado era $RANK"; FALHA=1; }
[ "${DES##*,}" = "$ANTIGO" ] || { echo "  FALHA: o desenho registrou o rank antigo ${DES##*,} e o forcado era $ANTIGO"; FALHA=1; }
[ "$NDRAW" = "0" ] || { echo "  FALHA: $NDRAW quadro(s) da cinematica nao descomprimiram"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "CINEMATICA DE RANK: FALHOU"
    exit 1
fi

echo
echo "CINEMATICA DE RANK: PASSOU (fases, zooms, posicoes, texturas e o fim em 2,2 s)"
