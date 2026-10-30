#!/bin/sh
# SWEEP: as 27 musicas x 3 dificuldades, uma por uma, no jogo real do PC.
#
# POR QUE ISTO EXISTE
# Todo o resto do harness prova UMA musica (o Tutorial). A Freeplay do port toca
# a primeira da lista. Resultado medido: 26 das 27 musicas e 54 dos 81 charts
# nunca tinham sido carregados pelo jogo -- um caminho de chart errado, um chart
# com BPM exotico ou uma secao que o loader nao aceita so' apareceria no console,
# com o jogo travado na tela do pendrive.
#
# O que cada rodada cobra (e de onde sai):
#   harness_start    o jogo comecou nessa musica (o gancho do harness)
#   chart_layout>=0  algum dos 4 layouts abriu o chart
#   tutorial_chart   status 0 e checksum != 0  -> o chart foi lido inteiro
#   play_total_notes > 0                       -> o chart tem nota
#   tutorial_audio_start >= 1                  -> o stems abriu e a musica anda
#   countdown_begin  + pelo menos 4 countdown_step
#   um estado 8 (a tela de jogo) desenhado
#   RESULTADO: PASSOU, nenhum estouro de pacote
#
# Uso: tools/run_sweep_harness.sh [quadros] [musicas]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-1200}
SOMENTE=${2:-}
# PS2H_BIN permite rodar o MESMO sweep com o binario dos sanitizadores:
# os 81 charts e os 12 palcos passam a ser exercitados com ASan/UBSan ligados,
# que e' onde erro de memoria aparece.
BIN=${PS2H_BIN:-build/fnf_harness}
ROOT=build/harness_assets
LOG="$ROOT/FNFPS2/FNF_COLHEITA.txt"

if [ ! -x "$BIN" ]; then
    echo "  sweep: sem harness construido; rode tools/run_harness.sh primeiro"
    exit 1
fi
python3 tools/preparar_sweep.py || exit 1

LISTA=$(ls assets-data/charts/*-chart.json | sed 's|.*/||; s|-chart.json||')
if [ -n "$SOMENTE" ]; then
    LISTA=$(echo "$SOMENTE" | tr ',' ' ')
fi

mkdir -p build/sweep
# indice de bs_stage_props[] -> id do palco, na ordem do plano (o log grava o
# indice; sem o nome ninguem sabe qual palco e').
python3 - <<'PYMAPA' > build/sweep/palcos.txt
import json
d = json.load(open("assets-data/stage-props.json"))
for i, st in enumerate(d["stages"]):
    print(i, st["id"])
PYMAPA
TOTAL=0
OK=0
FALHOU=""
AUDIO=0

for sid in $LISTA; do
    for diff in 0 1 2; do
        TOTAL=$((TOTAL + 1))
        rm -f "$LOG"
        out=$(FNF_START_SONG="$sid" FNF_START_DIFF="$diff" PS2H_FRAMES="$FRAMES" \
              PS2H_ROOT="$ROOT" PS2H_WATCHDOG=300 timeout 300 "$BIN" 2>&1)
        rc=$?
        motivo=""
        [ $rc -eq 0 ] || motivo="codigo de saida $rc"
        echo "$out" | grep -q "RESULTADO: PASSOU" || motivo="harness nao passou${motivo:+ ($motivo)}"

        campo() { awk -F, -v n="$1" -v c="$2" '$1=="E" && $3==n {print $c; exit}' "$LOG" 2>/dev/null; }
        hstart=$(campo harness_start 4)
        layout=$(campo chart_layout 4)
        cst=$(campo tutorial_chart 4)
        csum=$(campo tutorial_chart 5)
        notas=$(campo play_total_notes 4)
        cbegin=$(campo countdown_begin 4)
        nsteps=$(awk -F, '$1=="E" && $3=="countdown_step" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        naudio=$(awk -F, '$1=="E" && $3=="tutorial_audio_start" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nstate=$(awk -F, '$1=="S" && $3==8 {n++} END{print n+0}' "$LOG" 2>/dev/null)
        npalco=$(awk -F, '$1=="E" && $3=="palco_draw" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        palco=$(campo palco_entra 4)

        [ "${hstart:--1}" -ge 0 ] 2>/dev/null || motivo="nao comecou${motivo:+, $motivo}"
        [ "${layout:--1}" -ge 0 ] 2>/dev/null || motivo="layout do chart${motivo:+, $motivo}"
        [ "${cst:--1}" -eq 0 ] 2>/dev/null || motivo="chart nao carregou${motivo:+, $motivo}"
        [ "${csum:-0}" -ne 0 ] 2>/dev/null || motivo="checksum zero${motivo:+, $motivo}"
        [ "${notas:-0}" -gt 0 ] 2>/dev/null || motivo="chart sem notas${motivo:+, $motivo}"
        [ "${cbegin:-0}" -gt 0 ] 2>/dev/null || motivo="sem contagem${motivo:+, $motivo}"
        [ "${nsteps:-0}" -ge 4 ] 2>/dev/null || motivo="menos de 4 passos de contagem${motivo:+, $motivo}"
        [ "${nstate:-0}" -ge 1 ] 2>/dev/null || motivo="nao desenhou a tela de jogo${motivo:+, $motivo}"
        [ "${npalco:-0}" -ge 1 ] 2>/dev/null || motivo="nenhum prop de palco desenhado${motivo:+, $motivo}"

        if [ "${naudio:-0}" -ge 1 ]; then
            AUDIO=$((AUDIO + 1))
        else
            motivo="audio nao comecou${motivo:+, $motivo}"
        fi

        if [ -z "$motivo" ]; then
            OK=$((OK + 1))
            palco_nome=$(awk -v i="${palco:--1}" '$1==i {print $2; exit}' build/sweep/palcos.txt)
            echo "$palco_nome" >> build/sweep/palcos-vistos.txt
            printf "  ok: %-18s %3d notas, palco %-20s %d evento(s), audio em %d\n" \
                "$sid/$diff" "${notas:-0}" "${palco_nome:--}" \
                "$(awk -F, '$1=="E" && $3=="tutorial_chart_event" {n++} END{print n+0}' "$LOG")" \
                "${naudio:-0}"
        else
            echo "  FALHA: $sid/$diff: $motivo"
            cp "$LOG" "build/sweep/$sid-$diff.log" 2>/dev/null
            FALHOU="$FALHOU $sid/$diff"
        fi
    done
done

NPALCOS=$(sort -u build/sweep/palcos-vistos.txt 2>/dev/null | grep -c . || echo 0)
# Quantos palcos o conjunto de musicas testado USA. "desenhou 12" so' e' bom se
# forem 12 os que existem nessas musicas -- um palco que nunca desenha tem de
# falhar aqui, e nao passar despercebido.
ESPERADOS=$(awk -v lista="$LISTA" '
    BEGIN { n = split(lista, v, " "); for (i = 1; i <= n; i++) quer[v[i]] = 1 }
    quer[$1] { print $2 }
' build/sweep/mapa-musica-palco.txt | sort -u | grep -c . || echo 0)
FALTA=$(awk -v vistas="$NPALCOS" 'BEGIN { exit !(vistas < $1) }' 2>/dev/null; true)
NAO_DESENHOU=""
for st in $(awk -v lista="$LISTA" '
        BEGIN { n = split(lista, v, " "); for (i = 1; i <= n; i++) quer[v[i]] = 1 }
        quer[$1] { print $2 }' build/sweep/mapa-musica-palco.txt | sort -u); do
    grep -qx "$st" build/sweep/palcos-vistos.txt 2>/dev/null || NAO_DESENHOU="$NAO_DESENHOU $st"
done
rm -f build/sweep/palcos-vistos.txt
echo
echo "  sweep: $OK/$TOTAL combinacoes musica x dificuldade comecaram"
echo "         $NPALCOS/$ESPERADOS palco(s) usados pelas musicas desenharam"
if [ -n "$NAO_DESENHOU" ]; then
    echo "  FALHA: palco(s) que nenhuma musica conseguiu desenhar:$NAO_DESENHOU"
    exit 1
fi
echo "         (chart, notas, contagem, tela de jogo, palco e audio)"
if [ -n "$FALHOU" ]; then
    echo "  falharam:$FALHOU"
    exit 1
fi
