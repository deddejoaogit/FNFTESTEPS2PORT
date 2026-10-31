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
# Volta os charts dessas tres ao FCH3 antes do passe principal: o passe de
# eventos de animacao (mais abaixo) grava FCH4 no MESMO destino, e sem isto o
# "passe v3" seguinte rodaria v4 nessas tres sem ninguem notar.
restaurar_v3() {
    rm -f "$ROOT"/FNFPS2/assets/songs/{bopeebo,philly-nice,spookeez}/*.chart 2>/dev/null
    FNF_SWEEP_FMT=v3 python3 tools/preparar_sweep.py --musicas bopeebo,philly-nice,spookeez \
        >/dev/null 2>&1 || true
}
restaurar_v3
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
# Quantas notas de cada chart trazem NOTE KIND (`k`). O log do jogo tem de
# concordar com o JSON: no v5 o numero e' o do JSON, no v3/v4 e' zero, porque
# esses containers nao tem onde guardar o campo.
python3 - <<'PYKINDS' > build/sweep/kinds.txt
import json, glob, os
for f in sorted(glob.glob("assets-data/charts/*-chart.json")):
    sid = os.path.basename(f)[:-len("-chart.json")]
    d = json.load(open(f))
    for diff in ("easy", "normal", "hard"):
        notas = d.get("notes", {}).get(diff, []) or []
        com = sum(1 for n in notas if n.get("k"))
        sem = sum(1 for n in notas if n.get("k") == "non_scoreable")
        noa = sum(1 for n in notas if n.get("k") == "noanim")
        print(sid, diff, com, sem, noa)
PYKINDS
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
        # O OPONENTE: o personagem do slot dad, do pacote CHP4. Antes desta
        # build ele nao era desenhado em 26 das 27 musicas -- a tela mostrava
        # palco, BF, GF e mais ninguem. "carregado" = o pacote abriu e a tabela
        # de animacoes conferiu; "primeiro" = o sprite entrou no pacote de
        # desenho; "animando" = 10 quadros diferentes ja' subiram para a VRAM
        # (ou seja, nao e' um sprite congelado).
        # Animacao por EVENTO de chart (PlayAnimation: hey, cheer...). Conta as
        # que tocaram e as que o pacote nao tem -- as duas coisas sao verdade, e
        # a segunda e' dado, nao defeito: um chart pede 'cheer' no oponente e o
        # personagem daquela musica pode nao ter essa animacao.
        nanimtocou=$(awk -F, '$1=="E" && $3=="play_anim_tocou" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nanimsem=$(awk -F, '$1=="E" && $3=="play_anim_sem_frame" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nbfdraw=$(awk -F, '$1=="E" && $3=="char_bf_primeiro" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        ngfdraw=$(awk -F, '$1=="E" && $3=="char_gf_primeiro" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nbfanim=$(awk -F, '$1=="E" && $3=="char_bf_animando" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nlegado=$(awk -F, '$1=="E" && $3=="char_tutorial_legado" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nchardraw=$(awk -F, '$1=="E" && $3=="char_dad_primeiro" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        ncharanim=$(awk -F, '$1=="E" && $3=="char_dad_animando" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        # O Tutorial e' o unico caso em que o oponente E' a GF, e a GF do
        # tutorial ja' e' desenhada pelo caminho antigo (divergencia registrada
        # no plano, a resolver com captura de console). Ali o oponente
        # generico e' pulado de proposito: desenhar de novo seria ve-lo duas
        # vezes. O evento `char_dad_pulado` diz que foi isso, e nao uma falha.
        # "-hold": se o personagem do papel TEM animacao "-hold" (os olhos do
        # pai, a chama do Darnell), ela tem de aparecer. Alguns personagens nao
        # tem nenhuma, e aí nao haveria o que exigir.
        # O evento e' telemetry_event("char_hold_tem", papel, quantas) -> campo 4 = papel,
        # campo 5 = quantas. (A primeira versao testava o PAPEL > 0, e cobrava
        # "-hold" de personagem que nao tem nenhuma.)
        # O numero conta so' os "-hold" de `idle`/`sing*`/`dance*`: os de
        # taunt/uppercut/shoot vem de NOTE KINDS, e o script desses kinds nao
        # esta' no pacote de dados (ver docs/PLANO_COMPLETO.md, note kinds).
        nholdtem=$(awk -F, '$1=="E" && $3=="char_hold_tem" && $5+0>0 {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nhold=$(awk -F, '$1=="E" && $3=="char_hold" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        npulado=$(awk -F, '$1=="E" && $3=="char_dad_pulado" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        # NOTE KINDS: quantas notas chegaram com nome de kind, e quantas sao
        # `non_scoreable`/`noanim`. O esperado sai do JSON; ver build/sweep/kinds.txt.
        kcom=$(awk -F, '$1=="E" && $3=="chart_notas_com_kind" {print $4; exit}' "$LOG" 2>/dev/null)
        ksem=$(awk -F, '$1=="E" && $3=="chart_notas_com_kind" {print $5; exit}' "$LOG" 2>/dev/null)
        knoa=$(awk -F, '$1=="E" && $3=="chart_notas_noanim" {print $4; exit}' "$LOG" 2>/dev/null)
        case "$diff" in
            0) dn=easy ;;
            1) dn=normal ;;
            *) dn=hard ;;
        esac
        kexp=$(awk -v s="$sid" -v d="$dn" '$1==s && $2==d {print $3; exit}' build/sweep/kinds.txt 2>/dev/null)
        kexp=${kexp:-0}
        [ "${FNF_SWEEP_FMT:-v3}" = "v5" ] || kexp=0
        charruim=$(awk -F, '$1=="E" && ($3=="char_pacote_ruim" || $3=="char_sem_vram" || $3=="char_sem_staging") {n++} END{print n+0}' "$LOG" 2>/dev/null)
        palco=$(campo palco_entra 4)
        nicon=$(awk -F, '$1=="E" && $3=="evento_sem_icone" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nscript=$(awk -F, '$1=="E" && $3=="evento_sem_script" {n++} END{print n+0}' "$LOG" 2>/dev/null)

        [ "${hstart:--1}" -ge 0 ] 2>/dev/null || motivo="nao comecou${motivo:+, $motivo}"
        [ "${layout:--1}" -ge 0 ] 2>/dev/null || motivo="layout do chart${motivo:+, $motivo}"
        [ "${cst:--1}" -eq 0 ] 2>/dev/null || motivo="chart nao carregou${motivo:+, $motivo}"
        [ "${csum:-0}" -ne 0 ] 2>/dev/null || motivo="checksum zero${motivo:+, $motivo}"
        [ "${notas:-0}" -gt 0 ] 2>/dev/null || motivo="chart sem notas${motivo:+, $motivo}"
        [ "${cbegin:-0}" -gt 0 ] 2>/dev/null || motivo="sem contagem${motivo:+, $motivo}"
        [ "${nsteps:-0}" -ge 4 ] 2>/dev/null || motivo="menos de 4 passos de contagem${motivo:+, $motivo}"
        [ "${nstate:-0}" -ge 1 ] 2>/dev/null || motivo="nao desenhou a tela de jogo${motivo:+, $motivo}"
        # O `k` do JSON tem de chegar ao runtime: no v5, o numero de notas com
        # kind e' o do JSON; nos outros formatos ele TEM de ser zero (o campo
        # nao existe la'), e um zero no v5 seria o campo perdido de novo.
        [ "${kcom:-0}" -eq "${kexp:-0}" ] 2>/dev/null || motivo="note kinds: $kcom nota(s) com kind, esperado ${kexp:-0}${motivo:+, $motivo}"
        [ "${npalco:-0}" -ge 1 ] 2>/dev/null || motivo="nenhum prop de palco desenhado${motivo:+, $motivo}"
        if [ "${nholdtem:-0}" -ge 1 ] && [ "${nhold:-0}" -eq 0 ]; then
            motivo="o personagem tem animacao -hold e nunca entrou nela${motivo:+, $motivo}"
        fi
        if [ "${nlegado:-0}" -ge 1 ]; then
            # Tutorial: de proposito no caminho antigo (o unico conferido numa
            # captura de console). Exige os eventos do caminho antigo, e nao os
            # genericos -- senao a excecao passaria despercebida.
            [ "${nbfdraw:-0}" -eq 0 ] 2>/dev/null || motivo="o tutorial nao devia usar o BF generico${motivo:+, $motivo}"
        elif [ "${npulado:-0}" -ge 1 ]; then
            :
        else
            [ "${nchardraw:-0}" -ge 1 ] 2>/dev/null || motivo="o oponente nao foi desenhado${motivo:+, $motivo}"
            [ "${ncharanim:-0}" -ge 1 ] 2>/dev/null || motivo="o oponente nao animou${motivo:+, $motivo}"
        fi
        # Toda musica tem evento de animacao? Nao: so' algumas. Mas SOMA tem de
        # ser maior que zero nas 81, e isso e' conferido no fim (ANIM_TOTAL),
        # porque por combinacao o chart pode nao ter nenhum.
        ANIM_TOCou=$(( ${ANIM_TOCou:-0} + ${nanimtocou:-0} ))
        ANIM_SEM=$(( ${ANIM_SEM:-0} + ${nanimsem:-0} ))
        if [ "${nlegado:-0}" -eq 0 ]; then
            [ "${nbfdraw:-0}" -ge 1 ] 2>/dev/null || motivo="o BF nao foi desenhado${motivo:+, $motivo}"
            [ "${ngfdraw:-0}" -ge 1 ] 2>/dev/null || motivo="a GF nao foi desenhada${motivo:+, $motivo}"
            [ "${nbfanim:-0}" -ge 1 ] 2>/dev/null || motivo="o BF nao animou${motivo:+, $motivo}"
        fi
        [ "${charruim:-0}" -eq 0 ] 2>/dev/null || motivo="pacote ou VRAM do oponente falhou${motivo:+, $motivo}"
        # ESPAGUETE e' a unica musica com eventos que o port NAO reproduz, e os
        # dois grupos tem de aparecer no log: SetHealthIcon (a arte dos icones
        # nao esta' no pacote de dados) e os `sserafim*` (o script do palco nao
        # esta'). Se um dia a arte ou o script entrarem no repositorio, este
        # teste vira o lembrete de tirar o aviso -- e se os eventos pararem de
        # chegar, ele acusa a perda em vez de deixar o silencio passar.
        if [ "$sid" = "spaghetti" ] && [ "$diff" = "easy" ]; then
            if [ "${FNF_SWEEP_FMT:-v3}" = "v5" ]; then
                [ "${nicon:-0}" -ge 1 ] 2>/dev/null || motivo="SetHealthIcon nao chegou ao jogo${motivo:+, $motivo}"
                [ "${nscript:-0}" -ge 1 ] 2>/dev/null || motivo="evento de script nao chegou ao jogo${motivo:+, $motivo}"
            fi
        fi

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
echo "         (chart, notas, contagem, tela de jogo, palco, os tres personagens e audio)"
echo
echo "== eventos de animacao (PlayAnimation), com chart FCH4 =="
# O FCH3 (v3) tem UM u32 por evento: o nome da animacao se perde, entao no v3 o
# port sabe que houve o evento mas nao o que tocar. O FCH4 carrega a tabela de
# strings, e e' por ele que o nome chega. Aqui roda um passe separado nas
# musicas que TEM PlayAnimation (medido: bopeebo, philly-nice, spookeez; o
# tutorial tambem tem, mas ele esta' de proposito no caminho antigo).
ANIM_OK=0
ANIM_TOTAL=0
# --forcar: o `preparar_sweep` reaproveita o que ja' esta' no destino, e o
# destino tem os FCH3 do passe anterior. Sem isto o "passe v4" rodaria v3.
# --segundos 32 e --musicas: so' as tres musicas que TEM PlayAnimation, e com
# audio que alcanca o evento (o da philly-nice cai em 13,7 s; com 20 s de audio a
# musica terminava antes de chegar nele, e o port pulava o relogio para o FIM do
# chart quando o arquivo acabava -- o que parecia "evento nao consumido").
FNF_SWEEP_FMT=v4 python3 tools/preparar_sweep.py --forcar --segundos 32 \
    --musicas bopeebo,philly-nice,spookeez >/dev/null 2>&1 || {
    echo "  FALHA: nao consegui gerar os charts FCH4"; exit 1; }
for sid in bopeebo philly-nice spookeez; do
    for diff in 0 1 2; do
        ANIM_TOTAL=$((ANIM_TOTAL + 1))
        rm -f "$LOG"
        # O roteiro de teclas vem do PROPRIO chart: sem ele o jogador morre na
        # primeira sequencia de notas e o evento de animacao (em 13,7 s na
        # philly-nice) nunca chega a acontecer -- foi exatamente o que aconteceu
        # na primeira tentativa deste passe.
        DIFNOME=$(awk -v d="$diff" 'BEGIN{n=split("easy normal hard",v," ");print v[d+1]}')
        TECLAS=$(python3 tools/harness_presses.py \
                 "build/harness_assets/FNFPS2/assets/songs/$sid/$DIFNOME.chart" 2>/dev/null) || TECLAS=""
        out=$(FNF_START_SONG="$sid" FNF_START_DIFF="$diff" PS2H_FRAMES=1800 \
              PS2H_ROOT="$ROOT" PS2H_WATCHDOG=300 PS2H_INPUT="0:NONE" \
              PS2H_INPUT_AMOSTRAS="$TECLAS" timeout 300 "$BIN" 2>&1)
        echo "$out" | grep -q "RESULTADO: PASSOU" || { echo "  FALHA: $sid/$diff nao passou com FCH4"; exit 1; }
        n=$(awk -F, '$1=="E" && $3=="play_anim_tocou" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        sem=$(awk -F, '$1=="E" && $3=="play_anim_sem_frame" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        printf "  %-14s %s: %d animacao(oes) por evento tocaram, %d sem quadro\n" "$sid" "$diff" "${n:-0}" "${sem:-0}"
        [ "${n:-0}" -ge 1 ] || { echo "  FALHA: $sid/$diff nao tocou nenhuma animacao por evento"; exit 1; }
        ANIM_OK=$((ANIM_OK + 1))
    done
done
echo "  $ANIM_OK/$ANIM_TOTAL combinacoes tocaram animacao vinda do chart (FCH4)"
# e devolve essas tres ao FCH3, que e' o container do passe principal e do jogo
# que o console ja' provou.
restaurar_v3
if [ -n "$FALHOU" ]; then
    echo "  falharam:$FALHOU"
    exit 1
fi
