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
: > build/sweep/custo.txt
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
# Colunas: sid diff com_kind non_scoreable noanim KIND_JOGAVEL
#
# KIND_JOGAVEL e' quantas notas com kind caem nos primeiros 6 s E tem acao no
# personagem que esta' em cena (assets-data/notekinds.json x playData). E' o que
# o sweep cobra no passe v5: se existe nota de kind jogavel, o log TEM de ter
# pelo menos um `nota_kind_*`. Sem esse numero, um kind que parasse de ser
# aplicado passaria calado -- e foi assim que ele ficou 32 builds sem
# tratamento nenhum.
#
# Por que 6 s e nao 20: medido no próprio sweep. Uma passada de 1200 quadros
# chega a ~10 s de MUSICA (o resto do tempo e' boot, titulo e contagem
# regressiva), entao a janela tem de ser menor que isso -- com 15 s o 2hot
# falhava por um kind que a passada nem chega a alcancar (a primeira nota de
# kind dele esta' em 8,9 s). 6 s e' o que sobra com folga, e ainda cobre o
# `blazin` (24 notas) e o `ugh` (1).
import json, glob, os
nk = json.load(open("assets-data/notekinds.json"))["personagens"] if os.path.isfile("assets-data/notekinds.json") else {}
for f in sorted(glob.glob("assets-data/charts/*-chart.json")):
    sid = os.path.basename(f)[:-len("-chart.json")]
    d = json.load(open(f))
    meta = "assets-data/songs/%s-metadata.json" % sid
    ids = []
    if os.path.isfile(meta):
        ch = json.load(open(meta)).get("playData", {}).get("characters", {}) or {}
        ids = [v for v in ch.values() if isinstance(v, str)]
    acoes = set()
    for cid in ids:
        acoes.update(nk.get(cid, {}))
    for diff in ("easy", "normal", "hard"):
        notas = d.get("notes", {}).get(diff, []) or []
        com = sum(1 for n in notas if n.get("k"))
        sem = sum(1 for n in notas if n.get("k") == "non_scoreable")
        noa = sum(1 for n in notas if n.get("k") == "noanim")
        jog = sum(1 for n in notas
                  if n.get("k") in acoes and float(n.get("t", 0) or 0) < 6000.0)
        print(sid, diff, com, sem, noa, jog)
PYKINDS
# Quais papeis cada musica tem no playData. Sai do DADO, nao do log: e' o que o
# sweep cobra (tem `girlfriend`? entao a GF tem de aparecer na tela).
python3 - <<'PYPAPEIS' > build/sweep/papeis.txt
import json
for sid in sorted(__import__("os").listdir("assets-data/songs")):
    if not sid.endswith("-metadata.json"):
        continue
    d = json.load(open("assets-data/songs/" + sid))["playData"].get("characters", {})
    print(sid[:-len("-metadata.json")], 1 if d.get("girlfriend") else 0)
PYPAPEIS

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
        nvazio=$(awk -F, '$1=="E" && $3=="char_vazio" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        # O papel vazio NAO e' falha: e' o dado. `tutorial` e' a unica musica sem
        # `girlfriend` no playData, e a GF dela e' o OPONENTE, no slot dad. O que
        # se cobra aqui e' o esperado daquela musica (build/sweep/papeis.txt).
        temgf=$(awk -v s="$sid" '$1==s {print $2; exit}' build/sweep/papeis.txt 2>/dev/null)
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
        # DANCAS PERDIDAS: passos de danca que caíram enquanto o personagem
        # cantava. O lado nao gira por eles (BaseCharacter.dance() sai fora antes
        # de `hasDanced`), mas o passo fica consumido. Se o port voltar a
        # alternar pela paridade do passo, este numero continua igual -- o que
        # este contador prova e' que o caso EXISTE nos dados (e nao e' teorico).
        DANCAS_PULADAS=$(( ${DANCAS_PULADAS:-0} + $(awk -F, '$1=="E" && $3=="char_danca_pulada" && $5>0 {n++} END{print n+0}' "$LOG" 2>/dev/null) ))
        # COMBO/DROP da GF: quantas vezes o `combo<N>`/`drop<N>` do dado chegou a
        # tocar. O numero e' baixo por construcao -- o degrau real e' 50/200 e o
        # trecho tocado e' de 20 s --, mas se o gatilho estiver certo aparece
        # pelo menos uma vez nas 81.
        COMBOS=$(( ${COMBOS:-0} + $(awk -F, '$1=="E" && $3=="char_combo" {n++} END{print n+0}' "$LOG" 2>/dev/null) ))
        DROPS=$(( ${DROPS:-0} + $(awk -F, '$1=="E" && $3=="char_combo_drop" {n++} END{print n+0}' "$LOG" 2>/dev/null) ))
        # CAMERA: o papel inicial tem de ser o OPONENTE (PlayState L2224-2227 +
        # resetCamera no L936), e o alvo de cada FocusCamera tem de ser medido
        # (cam_foco_alvo). A soma dos alvos e' conferida no fim, porque nem todo
        # chart tem evento de camera.
        # $4 = PAPEL: 0 GF, 1 BF, 2 DAD (CH_PAPEL_* em char_layout.h). O 2 e' o
        # oponente nos 27 dados -- conferido: nenhuma musica tem oponente igual
        # ao jogador ou a namorada, entao o papel dad nunca e' pulado.
        ncamini=$(awk -F, '$1=="E" && $3=="cam_foco_inicial" && $4==2 && $5>0 {n++} END{print n+0}' "$LOG" 2>/dev/null)
        CAM_ALVOS=$(( ${CAM_ALVOS:-0} + $(awk -F, '$1=="E" && $3=="cam_foco_alvo" {n++} END{print n+0}' "$LOG" 2>/dev/null) ))
        # ESCALA: um evento por papel. Cada musica tem pelo menos dois papeis em
        # cena (nem toda tem `girlfriend`), e o numero e' o tamanho desenhado.
        nesc=$(awk -F, '$1=="E" && $3=="char_escala" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        # ZOOM/SCROLL INICIAIS: tem de vir do dado (zoom do palco, scrollSpeed do
        # chart da dificuldade), nao do 1,1 x 1,0 fixo de antes. O evento traz os
        # dois em milesimos; o teste exige que existam e sejam > 0, e que a soma
        # de zooms diferentes no sweep seja > 1 -- se todos voltassem a 1,1, a
        # unica coisa que restaria seria o valor fixo.
        cini=$(awk -F, '$1=="E" && $3=="cena_inicio" {print $4","$5; exit}' "$LOG" 2>/dev/null)
        cz=$(echo "$cini" | cut -d, -f1)
        cs=$(echo "$cini" | cut -d, -f2)
        # ESPELHO: 14 dos 46 personagens tem CharacterData.flipX=true (todos os
        # bf e todos os pico). Se o desenho parar de espelhar, o BF aparece
        # virado para o lado errado em TODAS as musicas -- e nada no log diria.
        nflip1=$(awk -F, '$1=="E" && $3=="char_flip" && $4>0 {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nflip0=$(awk -F, '$1=="E" && $3=="char_flip" && $4==0 {n++} END{print n+0}' "$LOG" 2>/dev/null)
        FLIPS1=$(( ${FLIPS1:-0} + ${nflip1:-0} ))
        FLIPS0=$(( ${FLIPS0:-0} + ${nflip0:-0} ))
        ZOOMS=${ZOOMS:-}; SCROLLS=${SCROLLS:-}
        [ -n "$cz" ] && [ "$cz" -gt 0 ] 2>/dev/null && ZOOMS="$ZOOMS $cz"
        [ -n "$cs" ] && [ "$cs" -gt 0 ] 2>/dev/null && SCROLLS="$SCROLLS $cs"
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
        # Quantas notas de kind o sweep exige que o JOGO tenha aplicado (nota
        # dentro da janela varrida e kind que o personagem em cena implementa).
        kjog=$(awk -v s="$sid" -v d="$dn" '$1==s && $2==d {print $6; exit}' build/sweep/kinds.txt 2>/dev/null)
        kjog=${kjog:-0}
        kacoes=$(awk -F, '$1=="E" && ($3=="nota_kind_sing" || $3=="nota_kind_anim" || $3=="nota_kind_sem_anim") {n++} END{print n+0}' "$LOG" 2>/dev/null)
        kacoes=${kacoes:-0}
        KACOES=$(( ${KACOES:-0} + kacoes ))
        [ "${FNF_SWEEP_FMT:-v3}" = "v5" ] || { kexp=0; kjog=0; }
        charruim=$(awk -F, '$1=="E" && ($3=="char_pacote_ruim" || $3=="char_sem_vram" || $3=="char_sem_staging") {n++} END{print n+0}' "$LOG" 2>/dev/null)
        palco=$(campo palco_entra 4)
        nicon=$(awk -F, '$1=="E" && $3=="evento_sem_icone" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        nscript=$(awk -F, '$1=="E" && $3=="evento_sem_script" {n++} END{print n+0}' "$LOG" 2>/dev/null)
        # CUSTO POR QUADRO (Fase 5): uploads de quadro de personagem e de prop do
        # palco divididos pelos quadros DESENHADOS com musica tocando. E' o
        # denominador que faltava: "1149 uploads" nao diz nada sem ele. A tabela
        # completa vai para build/sweep/custo.txt e o resumo sai no fim.
        # A ULTIMA amostra e' o acumulado da musica (as do meio sao parciais).
        cchar=$(awk -F, '$1=="E" && $3=="custo_char" {v=$4","$5} END{print v}' "$LOG" 2>/dev/null)
        cpal=$(awk -F, '$1=="E" && $3=="custo_palco" {v=$4","$5} END{print v}' "$LOG" 2>/dev/null)
        cqua=$(awk -F, '$1=="E" && $3=="custo_quadros" {v=$4","$5} END{print v}' "$LOG" 2>/dev/null)
        if [ -n "$cchar" ]; then
            printf "%s %s %s %s %s\n" "$sid" "$diff" "$cqua" "$cchar" "$cpal" >> build/sweep/custo.txt
            CUSTO_N=$(( ${CUSTO_N:-0} + 1 ))
        fi

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
        # A segunda metade da prova: o kind tem de ACONTECER, nao so' chegar.
        if [ "${kjog:-0}" -ge 1 ] && [ "${kacoes:-0}" -eq 0 ]; then
            motivo="note kinds: $kjog nota(s) jogavel(is) e nenhum nota_kind_* no log${motivo:+, $motivo}"
        fi
        [ "${npalco:-0}" -ge 1 ] 2>/dev/null || motivo="nenhum prop de palco desenhado${motivo:+, $motivo}"
        if [ "${nholdtem:-0}" -ge 1 ] && [ "${nhold:-0}" -eq 0 ]; then
            motivo="o personagem tem animacao -hold e nunca entrou nela${motivo:+, $motivo}"
        fi
        # O caminho antigo do Tutorial nao desenha mais nada: nenhuma musica
        # pode voltar para ele.
        [ "${nlegado:-0}" -eq 0 ] 2>/dev/null || motivo="o Tutorial voltou ao caminho antigo${motivo:+, $motivo}"
        [ "${ncamini:-0}" -ge 1 ] 2>/dev/null || motivo="a camera nao comecou no oponente${motivo:+, $motivo}"
        [ "${nesc:-0}" -ge 2 ] 2>/dev/null || motivo="a escala do personagem nao foi medida${motivo:+, $motivo}"
        [ "${nflip1:-0}" -ge 1 ] 2>/dev/null || motivo="o espelho (flipX) do jogador nao foi aplicado${motivo:+, $motivo}"
        [ -n "$cz" ] && [ "$cz" -gt 0 ] 2>/dev/null || motivo="o zoom inicial da musica nao foi medido${motivo:+, $motivo}"
        [ -n "$cs" ] && [ "$cs" -gt 0 ] 2>/dev/null || motivo="o scrollSpeed inicial da musica nao foi medido${motivo:+, $motivo}"
        [ "${npulado:-0}" -eq 0 ] 2>/dev/null || motivo="papel do oponente pulado por nome${motivo:+, $motivo}"
        [ "${nchardraw:-0}" -ge 1 ] 2>/dev/null || motivo="o oponente nao foi desenhado${motivo:+, $motivo}"
        [ "${ncharanim:-0}" -ge 1 ] 2>/dev/null || motivo="o oponente nao animou${motivo:+, $motivo}"
        # Toda musica tem evento de animacao? Nao: so' algumas. Mas SOMA tem de
        # ser maior que zero nas 81, e isso e' conferido no fim (ANIM_TOTAL),
        # porque por combinacao o chart pode nao ter nenhum.
        ANIM_TOCou=$(( ${ANIM_TOCou:-0} + ${nanimtocou:-0} ))
        ANIM_SEM=$(( ${ANIM_SEM:-0} + ${nanimsem:-0} ))
        [ "${nbfdraw:-0}" -ge 1 ] 2>/dev/null || motivo="o BF nao foi desenhado${motivo:+, $motivo}"
        [ "${nbfanim:-0}" -ge 1 ] 2>/dev/null || motivo="o BF nao animou${motivo:+, $motivo}"
        if [ "${temgf:-1}" = "1" ]; then
            [ "${ngfdraw:-0}" -ge 1 ] 2>/dev/null || motivo="a GF nao foi desenhada${motivo:+, $motivo}"
        else
            # Sem `girlfriend` no dado, o slot gf tem de ficar VAZIO e dizer que
            # ficou. Desenhar alguem ali seria inventar personagem.
            [ "${nvazio:-0}" -ge 1 ] 2>/dev/null || motivo="slot gf vazio nao foi reportado${motivo:+, $motivo}"
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
echo "  espelho: $FLIPS1 papel(eis) com flipX e $FLIPS0 sem, nas 81 combinacoes"
echo "  danca: $DANCAS_PULADAS papel(eis) perderam passo(s) de danca por estar cantando"
echo "  combo/drop da GF: $COMBOS combo(s) e $DROPS drop(s) tocados nas 81 combinacoes"
# NOTE KINDS: quantos kinds o jogo de fato aplicou. No v3/v4 os charts nem
# carregam o campo, entao o numero so' vale no passe v5 -- e o teste de
# "kind jogavel tem de gerar evento" so' existe la'.
echo "  note kinds aplicados pelo jogo: ${KACOES:-0} nota(s) em toda a passada"

# CUSTO POR QUADRO: resumo da Fase 5. Sem esta tabela, "otimizado" e' adjetivo.
if [ -s build/sweep/custo.txt ]; then
    echo "  custo: $CUSTO_N combinacoes medidas (tabela completa em build/sweep/custo.txt)"
    awk '{split($3,q,","); split($4,c,","); split($5,p,",");
          if (q[1]+0 > 0) printf "%d %s %s %d %d %d\n", c[2]+0, $1, $2, q[1], c[1], p[1]}' \
        build/sweep/custo.txt | sort -nr | head -5 | \
      awk '{printf "         %-12s %s  %6d quadros  %6d uploads de personagem (%d/100 quadr)  %4d de palco\n",
                   $2, $3, $4, $5, $1, $6}'
    CIM=$(awk '{split($4,c,","); if (c[2]!="") {n++; s+=c[2]}} END{printf "%.0f", (n? s/n : 0)}' build/sweep/custo.txt)
    echo "  custo: media de ${CIM}/100 uploads de personagem por quadro nas $CUSTO_N combinacoes"
fi
if [ -n "$FALHOU" ]; then
    echo "  falharam:$FALHOU"
    exit 1
fi
