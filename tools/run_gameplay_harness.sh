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
# 7600 quadros (127 s): a musica e o resultado cabem nisso e ainda sobram ~27 s
# de tela de resultado, o que e' o tempo necessario para o nome da musica
# RECIRCULAR duas vezes (o ciclo medido e' de ~12,9 s) -- sem isso o teste
# provaria so' a entrada.
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-7600}
# FNF_RANK=<0..5> forca o rank da tela de resultado (so' o harness tem esse
# gancho). As conferencias abaixo passam a usar o rank FORCADO: e' o unico jeito
# de exercitar as cinco artes do rankText, os cinco `getMusicDelay` e o par
# musica+intro -- o roteiro normal chega sempre no GREAT.
RANK_FORCADO=${FNF_RANK:--1}
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
# O PLACAR: o atlas carregou, os dez slots desenharam, e o numero montado dos
# digitos que ESTAO na tela tem de ser o placar da musica (`play_result_score`).
RESPLACARARTE=$(awk -F, '$1=="E" && $3=="resultado_placar_arte" {print $4; exit}' "$LOG")
RESPLACAR=$(awk -F, '$1=="E" && $3=="resultado_placar" {print $4; exit}' "$LOG")
RESFINAL=$(awk -F, '$1=="E" && $3=="resultado_placar_final" {print $4; exit}' "$LOG")
RESFINALMS=$(awk -F, '$1=="E" && $3=="resultado_placar_final" {print $5; exit}' "$LOG")
RESSCORE=$(awk -F, '$1=="E" && $3=="play_result_score" {print $4; exit}' "$LOG")
RESCLEARARTE=$(awk -F, '$1=="E" && $3=="resultado_clear_arte" {print $4; exit}' "$LOG")
RESCLEAR=$(awk -F, '$1=="E" && $3=="resultado_clear_final" {print $4; exit}' "$LOG")
RESCLEARMS=$(awk -F, '$1=="E" && $3=="resultado_clear_final" {print $5; exit}' "$LOG")
RESCLEARPCT=$(awk -F, '$1=="E" && $3=="play_result" {print $5; exit}' "$LOG")
RESRANKIDX=$(awk -F, '$1=="E" && $3=="resultado_rank" {print $4; exit}' "$LOG")
# A BARRA DO TOPO e a imagem da dificuldade. A barra nao tem arte (trapezio
# desenhado em codigo); o que se cobra e' que a IMAGEM da dificuldade da musica
# jogada tenha carregado e que a barra tenha assentado (borda de baixo nos
# valores que `BitmapUtil.createResultsBar` produz: ~74,2 e ~31,7 em 640x360).
RESDIFARTE=$(awk -F, '$1=="E" && $3=="resultado_dif_arte" {print $4; exit}' "$LOG")
RESDIFB=$(awk -F, '$1=="E" && $3=="resultado_dif_arte" {print $5; exit}' "$LOG")
# O CONTADOR PEQUENO do topo: entra no `getBFDelay()` do rank ja' com o valor
# final (nao ha' tween) e no x que vem da largura da imagem da dificuldade
# (`(difficulty.x + difficulty.width) + 60`, em decimos de px).
RESCLEARSMALL=$(awk -F, '$1=="E" && $3=="resultado_clear_small" {print $4; exit}' "$LOG")
RESCLEARSMALLX=$(awk -F, '$1=="E" && $3=="resultado_clear_small" {print $5; exit}' "$LOG")
# O NOME DA MUSICA. `resultado_nome_arte` traz 1 e o tamanho do bitmap do
# titulo; `resultado_nome` a posicao de entrada (468,5 / -30,0, em decimos);
# `resultado_nome_move` o primeiro quadro em que ele saiu do x de entrada;
# `resultado_nome_ciclo` cada recomeco do ciclo (`x + largura < 100`).
RESNOMEARTE=$(awk -F, '$1=="E" && $3=="resultado_nome_arte" {print $4; exit}' "$LOG")
RESNOMEB=$(awk -F, '$1=="E" && $3=="resultado_nome_arte" {print $5; exit}' "$LOG")
RESNOME=$(awk -F, '$1=="E" && $3=="resultado_nome" {print $4; exit}' "$LOG")
RESNOMEY=$(awk -F, '$1=="E" && $3=="resultado_nome" {print $5; exit}' "$LOG")
RESNOMEMOVE=$(awk -F, '$1=="E" && $3=="resultado_nome_move" {print $4; exit}' "$LOG")
RESNOMEMOVEMS=$(awk -F, '$1=="E" && $3=="resultado_nome_move" {print $5; exit}' "$LOG")
RESNOMECICLOS=$(awk -F, '$1=="E" && $3=="resultado_nome_ciclo" {n++} END{print n+0}' "$LOG")
# A MUSICA E OS SONS DA TELA DE RESULTADO. `resultado_musica_atraso` e' o
# `getMusicDelay` do rank; `resultado_musica` reporta cada passo do transporte
# (1 = intro comecou, 2 = principal em loop, 3 = trocou do intro para o loop no
# fim do intro). `resultado_ticks` conta os `scrollMenu` e `resultado_audio` os
# bytes que foram para o audsrv.
RESMUSATRASO=$(awk -F, '$1=="E" && $3=="resultado_musica_atraso" {print $4; exit}' "$LOG")
RESMUSEST=$(awk -F, '$1=="E" && $3=="resultado_musica" {print $4; exit}' "$LOG")
RESMUSINTRO=$(awk -F, '$1=="E" && $3=="resultado_musica" && $4==1 {n++} END{print n+0}' "$LOG")
RESMUSLOOP=$(awk -F, '$1=="E" && $3=="resultado_musica" && $4==2 {n++} END{print n+0}' "$LOG")
RESMUSTROCA=$(awk -F, '$1=="E" && $3=="resultado_musica" && $4==3 {n++} END{print n+0}' "$LOG")
RESTICKS=$(awk -F, '$1=="E" && $3=="resultado_ticks" {print $4; exit}' "$LOG")
RESAUDIO=$(awk -F, '$1=="E" && $3=="resultado_audio" {print $4; exit}' "$LOG")
RESBARRA=$(awk -F, '$1=="E" && $3=="resultado_barra" {print $4; exit}' "$LOG")
RESBARRA1=$(awk -F, '$1=="E" && $3=="resultado_barra" {print $5; exit}' "$LOG")
# O FLASH DA TELA (`bgFlash`): acende duas vezes -- em 37/24 s = 1542 ms (junto
# com a contagem do percentual, 5/24 s de queda) e de novo no `getFlashDelay()`
# do rank (14/24 s de queda). `campo` 0 = o da contagem, 1 = o do rank.
RESFLASHT=$(awk -F, '$1=="E" && $3=="resultado_flash" && $4==0 {print $5; exit}' "$LOG")
RESFLASHR=$(awk -F, '$1=="E" && $3=="resultado_flash" && $4==1 {print $5; exit}' "$LOG")
# Os CONTADORES DE JULGAMENTO: cada um reporta o numero que desenhou quando
# assenta. `campo` 0 = total acertado, 1 = maior combo, 2..5 = sick/good/bad/
# shit, 6 = erradas.
RESTALLYARTE=$(awk -F, '$1=="E" && $3=="resultado_tallies_arte" {print $4; exit}' "$LOG")
tally_de() {
    awk -F, -v c="$1" '$1=="E" && $3=="resultado_tally" && $4==c {print $5; exit}' "$LOG"
}
RESTALLY_0=$(tally_de 0)   # total acertado
RESTALLY_1=$(tally_de 1)   # maior combo
RESTALLY_2=$(tally_de 2)   # sick
RESTALLY_3=$(tally_de 3)   # good
RESTALLY_4=$(tally_de 4)   # bad
RESTALLY_5=$(tally_de 5)   # shit
RESTALLY_6=$(tally_de 6)   # erradas
RESTALLYVAL=$(awk -F, '$1=="E" && $3=="play_result_tallies" {print $4; exit}' "$LOG")
RESTALLYMISS=$(awk -F, '$1=="E" && $3=="play_result_tallies" {print $5; exit}' "$LOG")
RESMAXCOMBO=$(awk -F, '$1=="E" && $3=="play_result_score" {print $5; exit}' "$LOG")
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
echo "  placar: arte / slots ........ carregada=${RESPLACARARTE:-?} slots=${RESPLACAR:-?}"
echo "  contadores .................. arte=${RESTALLYARTE:-?} sick=${RESTALLY_2:-?} good=${RESTALLY_3:-?} bad=${RESTALLY_4:-?} shit=${RESTALLY_5:-?} miss=${RESTALLY_6:-?}"
echo "  percentual de conclusao ..... arte=${RESCLEARARTE:-?} na tela=${RESCLEAR:-?}% (o jogo calculou ${RESCLEARPCT:-?}%)"
echo "  percentual no topo .......... na tela=${RESCLEARSMALL:-?}% em x=${RESCLEARSMALLX:-?}.X (esperado 81% e 4215)"
echo "  flash da tela ............... contagem=${RESFLASHT:-?} ms / rank=${RESFLASHR:-?} ms (esperado 1542 e o mesmo ms da arte do rank)"
echo "  barra do topo ............... borda ${RESBARRA:-?}.X / ${RESBARRA1:-?}.X (esperado 742 / 317) imagem da dificuldade=${RESDIFARTE:-?} (${RESDIFB:-?} B)"
echo "  nome da musica .............. arte=${RESNOMEARTE:-?} (${RESNOMEB:-?} B) entrada=${RESNOME:-?}.X / ${RESNOMEY:-?}.X"
echo "  musica do resultado ......... atraso=${RESMUSATRASO:-?} ms, estado final=${RESMUSEST:-?} (1 = so' intro, 2 = loop direto, 3 = loop depois do intro)"
echo "  ticks / audio no audsrv ..... ${RESTICKS:-?} scrollMenu / ${RESAUDIO:-?} B"
echo "  nome: scroll / ciclos ....... saiu do lugar em ${RESNOMEMOVEMS:-?} ms a x=${RESNOMEMOVE:-?}.X, ${RESNOMECICLOS:-?} recomeco(s)"

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
# O `bgFlash` do inicio da contagem: `startRankTallySequence()` em 37/24 s.
if [ -z "${RESFLASHT:-}" ]; then
    echo "  FALHA: o flash da tela nunca acendeu na contagem do percentual"; FALHA=1
else
    [ "$RESFLASHT" -ge 1542 ] && [ "$RESFLASHT" -le 1582 ] || {
        echo "  FALHA: o flash da contagem acendeu em $RESFLASHT ms (o timer e' 37/24 s = 1542 ms)"; FALHA=1; }
fi
if [ -z "${RESRANKIDX:-}" ]; then
    echo "  FALHA: o rank nunca chegou a ser desenhado na tela de resultado"; FALHA=1
else
    ESP_RANK=$RANK
    [ "$RANK_FORCADO" -ge 0 ] && ESP_RANK=$RANK_FORCADO
    [ "$RESRANKIDX" = "$ESP_RANK" ] || { echo "  FALHA: a arte mostrada e' do rank $RESRANKIDX e o esperado era $ESP_RANK"; FALHA=1; }
    case "$ESP_RANK" in
        0) ESP_RANK_MS=7750 ;;   # 186/24
        1) ESP_RANK_MS=4458 ;;   # 107/24
        2) ESP_RANK_MS=4542 ;;   # 109/24
        3) ESP_RANK_MS=5083 ;;   # 122/24
        *) ESP_RANK_MS=5375 ;;   # 129/24 (PERFECT e PERFECT_GOLD)
    esac
    D=$((RESRANKMS - ESP_RANK_MS)); [ "$D" -lt 0 ] && D=$((-D))
    echo "  rank na tela ................ $RESRANKIDX aos ${RESRANKMS} ms (getFlashDelay = $ESP_RANK_MS ms)"
    # `displayRankText()` acende o flash no MESMO quadro em que o rank entra.
    [ -z "${RESFLASHR:-}" ] && { echo "  FALHA: o flash do rank nunca acendeu"; FALHA=1; }
    if [ -n "${RESFLASHR:-}" ]; then
        DF=$((RESFLASHR - RESRANKMS)); [ "$DF" -lt 0 ] && DF=$((-DF))
        [ "$DF" -le 20 ] || { echo "  FALHA: o flash do rank acendeu em $RESFLASHR ms e a arte do rank entrou em $RESRANKMS ms"; FALHA=1; }
    fi
    [ "$D" -le 40 ] || { echo "  FALHA: o rank entrou em ${RESRANKMS} ms, e o getFlashDelay do rank $ESP_RANK diz $ESP_RANK_MS"; FALHA=1; }
fi

# A MUSICA DO RESULTADO. `getMusicDelay` (Scoring.hx L530-547): 2/24 no SHIT,
# 3/24 no GOOD, 5/24 no GREAT, 0 no EXCELLENT e 95/24 no PERFECT. O `-intro`
# existe no SHIT e no EXCELLENT do bf e no EXCELLENT/PERFECT do pico; o GREAT de
# qualquer um dos dois NAO tem intro -- e o caso do roteiro normal.
ESP_MUS_MS=209; ESP_INTRO=0
[ "$RANK_FORCADO" -ge 0 ] && ESP_RANK_R=$RANK_FORCADO || ESP_RANK_R=$RANK
case "$ESP_RANK_R" in
    0) ESP_MUS_MS=84 ; ESP_INTRO=1 ;;   # 2/24,  resultsSHIT + intro
    1) ESP_MUS_MS=125; ESP_INTRO=0 ;;   # 3/24,  resultsNORMAL
    2) ESP_MUS_MS=209; ESP_INTRO=0 ;;   # 5/24,  resultsNORMAL
    3) ESP_MUS_MS=0  ; ESP_INTRO=1 ;;   # 0,     resultsEXCELLENT + intro
    *) ESP_MUS_MS=3959; ESP_INTRO=0 ;;  # 95/24, resultsPERFECT
esac
if [ -z "${RESMUSATRASO:-}" ]; then
    echo "  FALHA: a musica do resultado nunca foi pedida (getMusicDelay)"; FALHA=1
else
    [ "$RESMUSATRASO" = "$ESP_MUS_MS" ] || {
        echo "  FALHA: a musica foi pedida em ${RESMUSATRASO} ms e o getMusicDelay do rank $ESP_RANK_R diz $ESP_MUS_MS"; FALHA=1; }
fi
if [ "$ESP_INTRO" = 1 ]; then
    [ "${RESMUSINTRO:-0}" -ge 1 ] && [ "${RESMUSTROCA:-0}" -ge 1 ] || {
        echo "  FALHA: este rank tem intro e o transporte nao tocou intro->loop (intro=${RESMUSINTRO:-0} troca=${RESMUSTROCA:-0})"; FALHA=1; }
else
    [ "${RESMUSLOOP:-0}" -ge 1 ] || { echo "  FALHA: a musica principal (loop) nao comecou"; FALHA=1; }
fi
[ "${RESTICKS:-0}" -ge 10 ] || { echo "  FALHA: os ticks do percentual nao tocaram (${RESTICKS:-0} scrollMenu)"; FALHA=1; }
[ "${RESAUDIO:-0}" -gt 40000 ] || { echo "  FALHA: o anel entregou ${RESAUDIO:-0} B ao audsrv (esperado > 1 s de PCM)"; FALHA=1; }

# O PLACAR. A arte e' um atlas so'; sem ele os digitos nao aparecem. `slots`
# tem de ser os dez do ResultScore, e o NUMERO montado a partir dos digitos que
# estao desenhados tem de ser o placar que a musica fez -- e' o que prova que o
# indice (digito, quadro) e a posicao de cada slot estao certos.
[ "${RESPLACARARTE:-0}" -eq 1 ] || { echo "  FALHA: o atlas do placar nao carregou"; FALHA=1; }
[ "${RESPLACAR:-0}" -eq 10 ] || { echo "  FALHA: o placar desenhou ${RESPLACAR:-0} slots (esperado 10)"; FALHA=1; }
if [ -z "${RESFINAL:-}" ]; then
    echo "  FALHA: o placar nunca assentou na tela de resultado"; FALHA=1
else
    echo "  placar na tela .............. $RESFINAL aos ${RESFINALMS} ms (a musica fez ${RESSCORE:-?})"
    [ "$RESFINAL" = "${RESSCORE:-x}" ] || { echo "  FALHA: a tela mostra $RESFINAL e o placar da musica e' $RESSCORE"; FALHA=1; }
    # O slot 9 e' o ultimo a assentar: (i-1) + shuffle(41) + tween(23) +
    # finalDelay(max(0, digitos-8)) + os 5 quadros do piscar final, em 24 avos.
    DIG=0; N=${RESSCORE:-0}
    while [ "$N" -gt 0 ]; do DIG=$((DIG + 1)); N=$((N / 10)); done
    EXTRA=$((DIG - 8)); [ "$EXTRA" -lt 0 ] && EXTRA=0
    Q=$((8 + 41 + 23 + EXTRA + 5))
    ESP=$((Q * 1000 / 24))
    D=$((RESFINALMS - ESP)); [ "$D" -lt 0 ] && D=$((-D))
    [ "$D" -le 60 ] || { echo "  FALHA: o placar assentou em ${RESFINALMS} ms; os timers do ResultScore.hx dizem ~$ESP"; FALHA=1; }
fi

# OS CONTADORES DE JULGAMENTO. Sem a arte eles voltam a ser texto (e o harness
# falha, porque o teste roda com os assets do plano). Cada numero desenhado tem
# de ser o que o jogo contou -- `play_result_tallies` traz sick/good/bad/shit
# empacotados e as erradas no campo 2.
[ "${RESTALLYARTE:-0}" -eq 1 ] || { echo "  FALHA: o atlas dos contadores nao carregou"; FALHA=1; }
[ -n "${RESTALLY_0:-}" ] && [ -n "${RESTALLY_1:-}" ] && [ -n "${RESTALLY_2:-}" ] &&
    [ -n "${RESTALLY_3:-}" ] && [ -n "${RESTALLY_4:-}" ] && [ -n "${RESTALLY_5:-}" ] &&
    [ -n "${RESTALLY_6:-}" ] || { echo "  FALHA: algum contador nunca assentou na tela"; FALHA=1; }
if [ -n "${RESTALLYVAL:-}" ]; then
    E_SICK=$(( (RESTALLYVAL >> 24) & 255 ))
    E_GOOD=$(( (RESTALLYVAL >> 16) & 255 ))
    E_BAD=$(( (RESTALLYVAL >> 8) & 255 ))
    E_SHIT=$(( RESTALLYVAL & 255 ))
    E_HIT=$((E_SICK + E_GOOD + E_BAD + E_SHIT))
    [ "${RESTALLY_2:-x}" = "$E_SICK" ] || { echo "  FALHA: contador SICK na tela ${RESTALLY_2:-?}, jogo $E_SICK"; FALHA=1; }
    [ "${RESTALLY_3:-x}" = "$E_GOOD" ] || { echo "  FALHA: contador GOOD na tela ${RESTALLY_3:-?}, jogo $E_GOOD"; FALHA=1; }
    [ "${RESTALLY_4:-x}" = "$E_BAD" ]  || { echo "  FALHA: contador BAD na tela ${RESTALLY_4:-?}, jogo $E_BAD"; FALHA=1; }
    [ "${RESTALLY_5:-x}" = "$E_SHIT" ] || { echo "  FALHA: contador SHIT na tela ${RESTALLY_5:-?}, jogo $E_SHIT"; FALHA=1; }
    [ "${RESTALLY_6:-x}" = "${RESTALLYMISS:-y}" ] || { echo "  FALHA: contador MISS na tela ${RESTALLY_6:-?}, jogo ${RESTALLYMISS:-?}"; FALHA=1; }
    # `totalNotesHit` do original conta cada nota pontuavel acertada uma vez.
    [ "${RESTALLY_0:-x}" = "$E_HIT" ] || { echo "  FALHA: total acertado na tela ${RESTALLY_0:-?}, jogo $E_HIT"; FALHA=1; }
    [ "${RESTALLY_1:-x}" = "${RESMAXCOMBO:-y}" ] || { echo "  FALHA: maior combo na tela ${RESTALLY_1:-?}, jogo ${RESMAXCOMBO:-?}"; FALHA=1; }
fi

# O PERCENTUAL DE CONCLUSAO. A arte tem cinco pedacos (Left, Right, Small e os
# dois textos); o numero que o contador desenha tem de ser o que o jogo
# calculou (`play_result` campo 2), e ele so' aparece depois de 37/24 s.
[ "${RESCLEARARTE:-0}" -eq 1 ] || { echo "  FALHA: a arte do percentual nao carregou"; FALHA=1; }
if [ -z "${RESCLEAR:-}" ]; then
    echo "  FALHA: o percentual de conclusao nunca foi desenhado"; FALHA=1
else
    [ "$RESCLEAR" = "${RESCLEARPCT:-x}" ] || { echo "  FALHA: o percentual na tela e' $RESCLEAR e o jogo calculou $RESCLEARPCT"; FALHA=1; }
    [ "${RESCLEARMS:-0}" -ge 1542 ] || { echo "  FALHA: o percentual apareceu em ${RESCLEARMS} ms (antes do timer de 37/24 s)"; FALHA=1; }
fi

# O CONTADOR PEQUENO DO TOPO: mesmo numero do grande (o jogo calculou 81%) e no
# x do original -- (555 + 228 + 60) / 2 = 421,5 em 640x360.
if [ -z "${RESCLEARSMALL:-}" ]; then
    echo "  FALHA: o contador pequeno do topo nunca foi desenhado"; FALHA=1
else
    [ "$RESCLEARSMALL" = "${RESCLEARPCT:-x}" ] || {
        echo "  FALHA: o percentual no topo e' $RESCLEARSMALL% e o jogo calculou ${RESCLEARPCT:-?}%"; FALHA=1; }
    [ "${RESCLEARSMALLX:-0}" -ge 4205 ] && [ "${RESCLEARSMALLX:-0}" -le 4225 ] || {
        echo "  FALHA: o contador pequeno ficou em x=${RESCLEARSMALLX:-?} (esperado 4215)"; FALHA=1; }
fi

# A BARRA DO TOPO. A barra em si e' geometria (0 B de VRAM): o que se cobra e'
# que a borda de baixo tenha chegado aos valores do original em decimos de px
# (`borda_y0` = 74,2 e `borda_y1` = 31,7 em 640x360) e que a imagem da
# dificuldade jogada (normal no harness) tenha carregado.
[ "${RESDIFARTE:-0}" -eq 1 ] || { echo "  FALHA: a imagem da dificuldade nao carregou (${RESDIFARTE:-?})"; FALHA=1; }
if [ -z "${RESBARRA:-}" ]; then
    echo "  FALHA: a barra do topo nunca assentou"; FALHA=1
else
    [ "$RESBARRA" -ge 738 ] && [ "$RESBARRA" -le 746 ] || {
        echo "  FALHA: a borda esquerda da barra assentou em $RESBARRA (esperado 742)"; FALHA=1; }
    [ "$RESBARRA1" -ge 313 ] && [ "$RESBARRA1" -le 321 ] || {
        echo "  FALHA: a borda direita da barra assentou em $RESBARRA1 (esperado 317)"; FALHA=1; }
fi

# O NOME DA MUSICA: bitmap do titulo carregado, entrada em 468,5 / -30,0 (o
# `x = clearPercentSmall.x + 94` e o `y = -height` do `timerThenSongName`), o
# scroll comecando DEPOIS do `showSmallClearPercent` (+2,5 s do `getBFDelay`) e
# o ciclo recomecando duas vezes em 127 s.
[ "${RESNOMEARTE:-0}" -eq 1 ] || { echo "  FALHA: o nome da musica nao carregou a arte (${RESNOMEARTE:-?})"; FALHA=1; }
if [ -z "${RESNOME:-}" ]; then
    echo "  FALHA: o nome da musica nunca foi desenhado"; FALHA=1
else
    [ "$RESNOME" -ge 4675 ] && [ "$RESNOME" -le 4695 ] || {
        echo "  FALHA: o nome entrou em x=$RESNOME (esperado 4685)"; FALHA=1; }
    [ "${RESNOMEY:-0}" -ge -310 ] && [ "${RESNOMEY:-0}" -le -290 ] || {
        echo "  FALHA: o nome entrou em y=$RESNOMEY (esperado -300, y = -altura)"; FALHA=1; }
fi
if [ -z "${RESNOMEMOVE:-}" ]; then
    echo "  FALHA: o nome nunca comecou a andar"; FALHA=1
else
    [ "$RESNOMEMOVE" -lt 4680 ] || { echo "  FALHA: o nome reportou o scroll em x=$RESNOMEMOVE (nao saiu da entrada)"; FALHA=1; }
    [ "${RESNOMEMOVEMS:-0}" -ge 5800 ] || {
        echo "  FALHA: o nome comecou a andar em ${RESNOMEMOVEMS} ms (antes do getBFDelay + 2,5 s)"; FALHA=1; }
fi
[ "${RESNOMECICLOS:-0}" -ge 2 ] || {
    echo "  FALHA: o nome recomecou ${RESNOMECICLOS:-0} vez(es) em 127 s (esperado 2+)"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "GAMEPLAY: FALHOU"
    exit 1
fi

echo
echo "GAMEPLAY: PASSOU (acertos, placar, contagem, fim de musica e resultado no jogo real)"
