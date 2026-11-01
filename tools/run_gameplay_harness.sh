#!/bin/sh
# Execucao de gameplay do harness: prova que o caminho de ACERTO roda e que a
# musica TERMINA no resultado.
#
# Por que existe separado: o roteiro padrao do harness atravessa o jogo inteiro
# em 1400 quadros e passa ~100 quadros dentro do tutorial -- tempo de sobra para
# desenhar notas, e de menos para qualquer uma delas CHEGAR ao receptor. O
# julgamento (Scoring.hx, vida, combo, placar) nunca era executado no PC; os
# erros nesse caminho so' apareceriam no console.
#
# COMO O ROTEIRO E' GERADO (mudou duas vezes, e a segunda foi uma correcao):
#
#   1a versao, cadencia fixa: o periodo das pressoes e o espacamento das notas
#   tinham um divisor comum grande, entao o erro ficava preso em multiplos de
#   9.600 amostras -- nunca dentro da janela de 7.680. Zero acertos, 45 ghost
#   misses.
#
#   2a versao, cadencia pseudoaleatoria: provava o julgamento (9 acertos), mas
#   23 ghost misses. Cada ghost miss tira 0,08 de vida, entao o jogador MORRIA
#   no meio da musica -- o que ficou visivel quando o game over passou a
#   existir: o teste de fim de musica nunca chegava ao resultado.
#
#   versao atual: as pressoes saem do PROPRIO chart (tools/harness_presses.py),
#   uma por nota do jogador, no quadro em que a nota chega. Nada de aleatorio:
#   o roteiro e' deterministico e o acerto e' o esperado. Algumas notas sao
#   deixadas de fora de proposito para que o caminho de MISS tambem rode.
#
# O roteiro e' em AMOSTRAS, nao em quadros (PS2H_INPUT_AMOSTRAS): o jogo avisa o
# harness em que amostra a musica esta', e a tecla sai quando ela chega ali. Por
# quadro o roteiro erra assim que o tempo por quadro muda -- e muda, porque o
# relogio da musica anda por tempo e os DelayThread do audio tambem andam no
# tempo virtual do harness.
#
# Uso:  tools/run_gameplay_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-5600}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt
CHART=build/harness_assets/FNFPS2/assets/songs/tutorial/normal.chart
# Entrada no tutorial (as tres primeiras cruzes sao titulo -> menu -> story;
# a quarta entra no jogo).
ENTRADA="200:CROSS,420:CROSS,600:CROSS,760:CROSS,820:CROSS,880:CROSS,940:CROSS"

if [ ! -f "$CHART" ]; then
    echo "  FALHA: falta $CHART (rode tools/gen_harness_assets.py)"
    exit 1
fi
TECLAS=$(python3 tools/harness_presses.py "$CHART" --pular 3) || {
    echo "  FALHA: nao consegui gerar o roteiro a partir do chart"; exit 1; }
# grep -c . e nao wc -l: a ultima entrada nao tem quebra de linha, e wc -l
# contava uma a menos do que o roteiro tinha.
NOTAS=$(printf '%s' "$TECLAS" | tr ',' '\n' | grep -c .)
echo "== gameplay: apostando nas notas do chart ($NOTAS pressoes em amostras) =="

rm -f "$LOG"
PS2H_INPUT="$ENTRADA" PS2H_INPUT_AMOSTRAS="$TECLAS" \
    sh tools/run_harness.sh "$FRAMES" || exit 1

if [ ! -f "$LOG" ]; then
    echo "  FALHA: o jogo nao gravou $LOG"
    exit 1
fi

HITS=$(grep -c ',tutorial_note_hit,' "$LOG" 2>/dev/null || true)
[ -z "$HITS" ] && HITS=0
INICIOU=$(awk -F, '$1=="E" && $3=="tutorial_audio_start" {n++} END{print n+0}' "$LOG")
SCORE_MAX=$(awk -F, 'BEGIN{m=-1000000000} $1=="S" && $3==8 {if ($36+0 > m) m = $36+0} END{print m}' "$LOG")
SCORE_MIN=$(awk -F, 'BEGIN{m=1000000000} $1=="S" && $3==8 {if ($36+0 < m) m = $36+0} END{print m}' "$LOG")
SALTOS=$(awk -F, '$1=="S" && $3==8 {if (seen && $36 - prev >= 100) n++; prev=$36; seen=1} END{print n+0}' "$LOG")
CLOCK=$(awk -F, '$1=="S" && $3==8 {if ($27+0 > m) m = $27+0} END{print m+0}' "$LOG")
MISSES=$(awk -F, '$1=="E" && $3=="play_death" {n++} END{print n+0}' "$LOG")
# O ZOOM DO HUD (camHUD.zoom). O HUD do original inteiro -- notas, barra,
# icones, placar e a marca de versao -- esta' nessa camera, e ela sobe 3 % por
# compasso e decai todo quadro (PlayState.hx L1227, L1846-1859).
# ICONES DE VIDA. Os dois existem no original em TODA musica (PlayState.hx
# L2155 jogador-2, L2190 jogador-1), com a arte do `healthIcon` do personagem e
# 'face' quando ele nao declara um. Antes desta build o HUD mostrava dois
# retangulos do atlas do tutorial: a arte existia no repositorio oficial e nunca
# tinha sido empacotada.
ICONES=$(awk -F, '$1=="E" && $3=="icone_carregado" {n++} END{print n+0}' "$LOG")
# ULTIMO valor: o jogo publica `icone_desenho` tambem durante a musica (a cada
# 300 quadros), e o que interessa aqui e' o total do fim da rodada.
ICONEDES=$(awk -F, '$1=="E" && $3=="icone_desenho" {v=$4} END{print v}' "$LOG")
ICONEEST=$(awk -F, '$1=="E" && $3=="icone_estado" {n++} END{print n+0}' "$LOG")
HUDZOOM=$(awk -F, '$1=="E" && $3=="hud_zoom" {print $4; exit}' "$LOG")
HUDPULSOS=$(awk -F, '$1=="E" && $3=="hud_zoom" {print $5; exit}' "$LOG")

# NOTE SPLASH: o brilho do acerto SICK. `splash_arte` diz que a regiao foi
# carregada para dentro do atlas do HUD; `splash_novo` sai uma vez por splash e
# `splash_ativos` so' quando o numero na tela muda.
SPLARTE=$(awk -F, '$1=="E" && $3=="splash_arte" {print $4; exit}' "$LOG")
SPLNOVOS=$(grep -c ',splash_novo,' "$LOG" 2>/dev/null || true)
[ -z "$SPLNOVOS" ] && SPLNOVOS=0
SPLMAX=$(awk -F, '$1=="E" && $3=="splash_ativos" {if ($4+0 > m) m=$4+0} END{print m+0}' "$LOG")
SPLSICK=$(awk -F, '$1=="E" && $3=="play_result_tallies" {print int(($4+0)/16777216); exit}' "$LOG")

RESULT=$(awk -F, '$1=="S" && $3==9 {n++} END{print n+0}' "$LOG")
RANK=$(awk -F, '$1=="E" && $3=="play_result" {print $4; exit}' "$LOG")
PERC=$(awk -F, '$1=="E" && $3=="play_result" {print $5; exit}' "$LOG")
DRAW=$(awk -F, '$1=="E" && $3=="result_draw" {print $4; exit}' "$LOG")
# TELA DE RESULTADO: a arte (fundo + 3 folhas) e os TIMERS do ResultState.hx
# (6/24 s, 21/24 s e 36/24 s = 250, 875 e 1500 ms depois de a tela abrir).
RESARTE=$(awk -F, '$1=="E" && $3=="resultado_arte" {print $4; exit}' "$LOG")
RESFUNDO=$(awk -F, '$1=="E" && $3=="resultado_fundo" {print $4; exit}' "$LOG")
RESCENA=$(awk -F, '$1=="E" && $3=="resultado_cena" {print $4; exit}' "$LOG")
RESF1=$(awk -F, '$1=="E" && $3=="resultado_folhas" && $4==1 {print $5; exit}' "$LOG")
RESF2=$(awk -F, '$1=="E" && $3=="resultado_folhas" && $4==2 {print $5; exit}' "$LOG")
RESF3=$(awk -F, '$1=="E" && $3=="resultado_folhas" && $4==3 {print $5; exit}' "$LOG")
# ARTE DO RANK: o par (vertical + horizontal) do rank conquistado e o instante
# em que ele entra (`Scoring.hx` getFlashDelay dividido por 24).
RESRANKARTE=$(awk -F, '$1=="E" && $3=="resultado_rank_arte" {print $4; exit}' "$LOG")
RESRANKVISTO=$(awk -F, '$1=="E" && $3=="resultado_rank_arte" {print $5; exit}' "$LOG")
RESRANKIDX=$(awk -F, '$1=="E" && $3=="resultado_rank" {print $4; exit}' "$LOG")
RESRANKMS=$(awk -F, '$1=="E" && $3=="resultado_rank" {print $5; exit}' "$LOG")
CDFIM=$(awk -F, '$1=="E" && $3=="countdown_step" && $4==5 {print $2; exit}' "$LOG")
CDPASSOS=$(awk -F, '$1=="E" && $3=="countdown_step" {n++} END{print n+0}' "$LOG")
CDBEAT=$(awk -F, '$1=="E" && $3=="countdown_begin" {print $4; exit}' "$LOG")

PALCODRAW=$(awk -F, '$1=="E" && $3=="palco_draw" {if ($4+0 > m) m = $4+0} END{print m+0}' "$LOG")
PALCOFALHA=$(awk -F, '$1=="E" && ($3=="palco_falha" || $3=="palco_sem_vram") {n++} END{print n+0}' "$LOG")
PALCOPOOL=$(awk -F, '$1=="E" && $3=="palco_pool" {print $4; exit}' "$LOG")

echo
echo "  musica iniciada ............. $INICIOU vez(es)"
echo "  props do palco desenhados ... $PALCODRAW (falhas: $PALCOFALHA, livre no pool: ${PALCOPOOL:-?})"
echo "  relogio maximo da musica ..... $CLOCK amostras"
echo "  acertos julgados ............. $HITS de $NOTAS pressoes"
echo "  placar (minimo..maximo) ...... $SCORE_MIN .. $SCORE_MAX"
echo "  acertos que somaram pontos ... $SALTOS"
echo "  mortes (game over) ........... $MISSES"
echo "  quadros na tela de resultado . $RESULT"
echo "  rank / clear percent ......... ${RANK:-?} / ${PERC:-?}%"
echo "  quads desenhados na tela 9 ... ${DRAW:-?}"
echo "  batida / passos da contagem .. ${CDBEAT:-?} amostras / ${CDPASSOS:-?}"
echo "  HUD: zoom maximo / pulsos .... ${HUDZOOM:-?} (milesimos) / ${HUDPULSOS:-?}"
echo "  icones de vida ............... ${ICONES:-?} carregado(s), ${ICONEDES:-?} desenho(s), ${ICONEEST:-?} troca(s) de estado"
echo "  note splash .................. arte=${SPLARTE:-?} criadas=$SPLNOVOS (SICK=$SPLSICK) pico na tela=$SPLMAX"
echo "  tela de resultado: arte ..... folhas=${RESARTE:-?} fundo=${RESFUNDO:-?} cena=${RESCENA:-?}"
echo "  folhas na tela em (ms) ...... ${RESF1:-?} / ${RESF2:-?} / ${RESF3:-?} (esperado 250 / 875 / 1500)"
echo "  arte do rank ................ carregada=${RESRANKARTE:-?} (${RESRANKVISTO:-?} B)"

FALHA=0
[ "$CLOCK" -gt 100000 ] || { echo "  FALHA: o relogio da musica nao andou (o transporte parou)"; FALHA=1; }
[ "$HITS" -ge 20 ]      || { echo "  FALHA: menos de 20 acertos com o roteiro vindo do chart"; FALHA=1; }
[ "$SALTOS" -ge 20 ]    || { echo "  FALHA: os acertos nao somaram pontos"; FALHA=1; }
[ "$SCORE_MAX" -gt 5000 ] || { echo "  FALHA: placar maximo baixo demais para tantos acertos"; FALHA=1; }
# Nenhuma morte: o roteiro acerta as notas, entao a vida sobe. Uma morte aqui
# significa que o alinhamento amostra->quadro saiu errado.
[ "$MISSES" -eq 0 ]     || { echo "  FALHA: o jogador morreu mesmo acertando as notas do chart"; FALHA=1; }

# A tela de resultado tem de aparecer sozinha quando a musica termina, com o
# rank calculado pelo mesmo nucleo que julgou as notas.
[ "$RESULT" -ge 2 ] || { echo "  FALHA: o jogo nao chegou na tela de resultado"; FALHA=1; }
[ -n "$RANK" ]      || { echo "  FALHA: nenhum play_result no log"; FALHA=1; }
[ "$RANK" != "-1" ]  || { echo "  FALHA: rank NONE -- o total de notas do chart chegou zerado no julgamento"; FALHA=1; }
[ "${DRAW:-0}" -gt 0 ] || { echo "  FALHA: a tela de resultado nao desenhou nada"; FALHA=1; }
[ "${CDPASSOS:-0}" -ge 5 ] || { echo "  FALHA: a contagem regressiva nao passou pelos cinco passos"; FALHA=1; }
# O palco generico: os props do mainStage vem dos pacotes CFZ4 e moram no pool.
# Zero props desenhados significa que o jogo caiu na textura composta -- o que
# e' aceitavel como reserva, mas nao como resultado do teste.
[ "${PALCODRAW:-0}" -ge 1 ] || { echo "  FALHA: nenhum prop do palco foi desenhado (o jogo ficou na textura composta)"; FALHA=1; }
[ "${PALCOFALHA:-1}" -eq 0 ] || { echo "  FALHA: $PALCOFALHA falha(s) de prop de palco (pacote ou VRAM)"; FALHA=1; }
[ -n "${CDBEAT:-}" ] && [ "$CDBEAT" -gt 0 ] || { echo "  FALHA: a batida da contagem saiu zero"; FALHA=1; }
if [ -n "${CDFIM:-}" ]; then
    awk -F, -v fim="$CDFIM" '$1=="E" && $3=="tutorial_audio_start" {if ($2+0 <= fim+0) {print "  FALHA: a musica comecou antes do fim da contagem"; exit 1} exit 0}' "$LOG" || FALHA=1
else
    echo "  FALHA: a contagem regressiva nunca terminou (sem passo AFTER)"; FALHA=1
fi

# O HUD tem de ter piscado: no tutorial (100 BPM, um pulso por compasso de 4
# batidas) sao ~29 pulsos em 25 s de musica. Exigir 20 deixa folga para a
# janela medida sem aceitar "nunca pulsou".
[ "${HUDZOOM:-0}" -gt 1020 ] || { echo "  FALHA: o zoom do HUD nunca subiu (hud_zoom=$HUDZOOM)"; FALHA=1; }
[ "${HUDPULSOS:-0}" -ge 20 ] || { echo "  FALHA: o HUD pulsou menos de 20 vezes ($HUDPULSOS)"; FALHA=1; }
# Os DOIS icones tem de carregar, aparecer no pacote de desenho e nao ficar
# congelados: o oponente sai de "idle" quando a vida passa do limiar (o roteiro
# do tutorial deixa a vida cair e subir).
[ "${ICONES:-0}" -ge 2 ] || { echo "  FALHA: menos de dois icones de vida carregaram ($ICONES)"; FALHA=1; }
[ "${ICONEDES:-0}" -ge 100 ] || { echo "  FALHA: os icones quase nao foram desenhados ($ICONEDES)"; FALHA=1; }
[ "${ICONEEST:-0}" -ge 1 ] || { echo "  FALHA: nenhum icone trocou de estado (idle/losing/winning)"; FALHA=1; }

# NOTE SPLASH. Tres exigencias, cada uma sobre um numero diferente:
#   * a arte carregou (sem ela o jogo nao desenha nada e o log diria 0);
#   * uma splash por SICK, EXATAMENTE -- e' o que prova que ela nao aparece em
#     GOOD/BAD/SHIT (o original so' a cria em `doesNotesplash`, que e' SICK) nem
#     falta em algum SICK;
#   * o pico na tela e' >= 1 (ela chega a ser desenhada) e <= o pool
#     (SP_MAX = 24), que e' o limite que o teste do PC conhece.
[ "${SPLARTE:-0}" -eq 1 ] || { echo "  FALHA: a arte do note splash nao carregou (splash_arte=$SPLARTE)"; FALHA=1; }
[ "$SPLNOVOS" -gt 0 ]      || { echo "  FALHA: nenhuma splash foi criada apesar de haver SICKs"; FALHA=1; }
[ "$SPLNOVOS" -eq "${SPLSICK:--1}" ] || { echo "  FALHA: $SPLNOVOS splash(es) para $SPLSICK SICK(s) -- tem de ser uma por SICK"; FALHA=1; }
[ "${SPLMAX:-0}" -ge 1 ]  || { echo "  FALHA: nenhuma splash chegou a ser desenhada"; FALHA=1; }
[ "${SPLMAX:-0}" -le 24 ] || { echo "  FALHA: $SPLMAX splashes na tela passa do pool (24)"; FALHA=1; }

# TELA DE RESULTADO. A arte tem de ter carregado (3 folhas + o gradiente) e as
# folhas tem de aparecer nos TIMERS do ResultState.hx -- nao antes, nao depois.
# A tolerancia e' de 2 quadros (33 ms): o evento sai no quadro em que a folha
# entra e o relogio da tela e' lido por quadro.
[ "${RESARTE:-0}" -eq 3 ] || { echo "  FALHA: a arte da tela de resultado nao carregou ($RESARTE de 3 folhas)"; FALHA=1; }
[ "${RESFUNDO:-0}" -eq 1 ] || { echo "  FALHA: o gradiente do fundo da tela de resultado nao carregou"; FALHA=1; }
# cena = folhas | (fundo << 8) no campo 4 do evento.
[ "${RESCENA:-0}" -eq 259 ] || { echo "  FALHA: a cena da tela de resultado saiu incompleta (cena=$RESCENA, esperado 259)"; FALHA=1; }
for par in "1:250:${RESF1:-}" "2:875:${RESF2:-}" "3:1500:${RESF3:-}"; do
    n=${par%%:*}; resto=${par#*:}; quer=${resto%%:*}; tem=${resto#*:}
    if [ -z "$tem" ]; then echo "  FALHA: a folha $n nunca apareceu"; FALHA=1; continue; fi
    d=$((tem - quer)); [ "$d" -lt 0 ] && d=$((-d))
    [ "$d" -le 40 ] || { echo "  FALHA: a folha $n entrou em ${tem} ms, e o timer do original diz $quer"; FALHA=1; }
done

# ARTE DO RANK. Tres exigencias:
#   * o par carregou (sem ele o rank fica so' no texto);
#   * o rank desenhado e' o MESMO que o jogo calculou (`play_result`);
#   * o rank entra no `getFlashDelay` da fonte -- 129/24 s no PERFECT, 122 no
#     EXCELLENT, 109 no GREAT, 107 no GOOD e 186 no SHIT (Scoring.hx L568-586).
[ "${RESRANKARTE:-0}" -eq 1 ] || { echo "  FALHA: a arte do rank nao carregou"; FALHA=1; }
[ "${RESRANKVISTO:-0}" -gt 20000 ] || { echo "  FALHA: a arte do rank carregou vazia (${RESRANKVISTO:-0} B)"; FALHA=1; }
if [ -z "${RESRANKIDX:-}" ]; then
    echo "  FALHA: o rank nunca chegou a ser desenhado na tela de resultado"; FALHA=1
else
    [ "$RESRANKIDX" = "$RANK" ] || { echo "  FALHA: a arte mostrada e' do rank $RESRANKIDX e o jogo calculou $RANK"; FALHA=1; }
    case "$RANK" in
        0) ESP_RANK_MS=7750 ;;   # 186/24
        1) ESP_RANK_MS=4458 ;;   # 107/24
        2) ESP_RANK_MS=4542 ;;   # 109/24
        3) ESP_RANK_MS=5083 ;;   # 122/24
        *) ESP_RANK_MS=5375 ;;   # 129/24 (PERFECT e PERFECT_GOLD)
    esac
    D=$((RESRANKMS - ESP_RANK_MS)); [ "$D" -lt 0 ] && D=$((-D))
    echo "  rank na tela ................ $RESRANKIDX aos ${RESRANKMS} ms (getFlashDelay = $ESP_RANK_MS ms)"
    [ "$D" -le 40 ] || { echo "  FALHA: o rank entrou em ${RESRANKMS} ms, e o getFlashDelay do rank $RANK diz $ESP_RANK_MS"; FALHA=1; }
fi

if [ "$FALHA" != 0 ]; then
    echo
    echo "GAMEPLAY: FALHOU"
    exit 1
fi

echo
echo "GAMEPLAY: PASSOU (acertos, placar, contagem, fim de musica e resultado no jogo real)"
