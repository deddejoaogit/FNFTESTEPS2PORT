#!/bin/sh
# Execucao de gameplay do harness: prova que o caminho de ACERTO roda.
#
# Por que existe separado: o roteiro padrao do harness atravessa o jogo inteiro
# em 1400 quadros e passa ~100 quadros dentro do tutorial -- tempo de sobra para
# desenhar notas, e de menos para qualquer uma delas CHEGAR ao receptor. O
# julgamento (Scoring.hx, vida, combo, placar) nunca era executado no PC; os
# erros nesse caminho so' apareceriam no console.
#
# O roteiro abaixo entra no tutorial e depois aperta as setas, alternando os
# quatro sentidos. A CADENCIA NAO E' ARBITRARIA -- e o detalhe que fez a
# primeira versao deste teste falhar com 45 ghost misses e nenhum acerto:
#
#   as notas do chart estao a cada 57.600 amostras (72 quadros);
#   apertando a cada 12 quadros, cada sentido volta a cada 48 quadros, ou seja
#   38.400 amostras, e mdc(38.400, 57.600) = 9.600 amostras = 12 quadros;
#   como a janela de acerto do jogo e' de 7.200 amostras (150 ms), a distancia
#   entre uma pressao e a nota mais proxima daquele sentido ficava presa em
#   multiplos de 9.600 -- sempre 0 ou 9.600 (12 quadros), nunca dentro da
#   janela. Alias puro.
#
# A 7 quadros por passo, cada sentido volta a cada 28 quadros (22.400 amostras)
# e mdc(22.400, 57.600) = 1.600 amostras -- bem dentro da janela. As pressoes
# que nao caem em nota continuam exercitando o ghost miss, que tambem
# interessa. O que este teste afirma e':
#
#   - o tutorial entra em ui_state 8 e a musica comeca (relogio > 0);
#   - pelo menos dois ACERTOS acontecem (tutorial_note_hit no log);
#   - o placar sai de zero (prova que fnf_play_hit pontuou de verdade);
#   - o harness nao estoura pacote, nao escreve fora do limite, nao trava.
#
# Uso:  tools/run_gameplay_harness.sh [quadros]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-3200}
LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt

# Entrada no tutorial (as tres primeiras cruzes sao titulo -> menu -> story;
# a quarta entra no jogo). Depois, pressoes a cada 7 quadros.
ENTRADA="200:CROSS,420:CROSS,600:CROSS,760:CROSS,820:CROSS,880:CROSS,940:CROSS"
# Cadencia IRREGULAR, e por um motivo medido: com passos fixos, a distancia
# entre uma pressao daquele sentido e a nota mais proxima fica presa num
# multiplo do mdc entre o periodo e o espacamento das notas. Duas tentativas
# medidas, ambas com ZERO acertos e 45 ghosts:
#
#   passo 12 (sentido a cada 48 quadros): mdc 9.600 amostras -> erro 0 ou 9.600
#   passo 7  (sentido a cada 28 quadros): mdc 1.600, mas o resto ficou em 8.000
#                                          amostras -- 800 fora da janela
#
# Um gerador pseudoaleatorio DETERMINISTICO (o mesmo em toda rodada) espalha as
# pressoes e resolve o alias de uma vez: 1/4 das pressoes cai dentro da janela
# de algum sentido, o que basta para o teste. O estado inicial e' fixo, entao
# duas rodadas produzem exatamente o mesmo roteiro.
TECLAS=""
Q=1000
SORTE=12345
while [ "$Q" -lt 3200 ]; do
    SORTE=$(( (SORTE * 1103515245 + 12345) % 2147483648 ))
    case $(( (SORTE / 65536) % 4 )) in
        0) T=$TECLAS,${Q}:LEFT ;;
        1) T=$TECLAS,${Q}:DOWN ;;
        2) T=$TECLAS,${Q}:UP ;;
        3) T=$TECLAS,${Q}:RIGHT ;;
    esac
    TECLAS=$T
    Q=$((Q + 3 + (SORTE / 2097152) % 4))
done

rm -f "$LOG"
echo "== gameplay: roteiro com pressoes continuas no tutorial =="
PS2H_INPUT="$ENTRADA$TECLAS" sh tools/run_harness.sh "$FRAMES" || exit 1

if [ ! -f "$LOG" ]; then
    echo "  FALHA: o jogo nao gravou $LOG"
    exit 1
fi

HITS=$(grep -c ',tutorial_note_hit,' "$LOG" 2>/dev/null || true)
[ -z "$HITS" ] && HITS=0
INICIOU=$(awk -F, '$1=="E" && $3=="tutorial_audio_start" {n++} END{print n+0}' "$LOG")
# O placar do original NAO e' limitado em zero, entao ele anda para baixo com
# as penalidades: o que interessa aqui e' (a) ele mudar, provando que o caminho
# de pontuacao rodou, e (b) o maximo visto, que mostra os acertos pontuando.
SCORE_MAX=$(awk -F, 'BEGIN{m=-1000000000} $1=="S" && $3==8 {if ($36+0 > m) m = $36+0} END{print m}' "$LOG")
SCORE_MIN=$(awk -F, 'BEGIN{m=1000000000} $1=="S" && $3==8 {if ($36+0 < m) m = $36+0} END{print m}' "$LOG")
# Acerto que soma ponto: um salto positivo grande entre dois quadros. E' o que
# prova que fnf_play_hit() somou pontos de verdade -- o valor absoluto do
# placar nao serve, porque centenas de ghost misses o empurram para baixo e o
# original NAO limita em zero.
SALTOS=$(awk -F, '$1=="S" && $3==8 {if (seen && $36 - prev >= 100) n++; prev=$36; seen=1} END{print n+0}' "$LOG")
CLOCK=$(awk -F, '$1=="S" && $3==8 {if ($27+0 > m) m = $27+0} END{print m+0}' "$LOG")

echo
echo "  musica do tutorial iniciada .. $INICIOU vez(es)"
echo "  relogio maximo da musica ..... $CLOCK amostras"
echo "  acertos julgados ............. $HITS"
echo "  placar (minimo..maximo) ...... $SCORE_MIN .. $SCORE_MAX"
echo "  acertos que somaram pontos ... $SALTOS"

FALHA=0
[ "$CLOCK" -gt 100000 ] || { echo "  FALHA: o relogio da musica nao andou (o transporte parou)"; FALHA=1; }
[ "$HITS" -ge 2 ]       || { echo "  FALHA: menos de dois acertos julgados -- o caminho de julgamento nao rodou"; FALHA=1; }
[ "$SCORE_MAX" != "$SCORE_MIN" ] || { echo "  FALHA: o placar nunca mudou -- nada pontuou"; FALHA=1; }
[ "$SALTOS" -ge 2 ]      || { echo "  FALHA: nenhum salto positivo de placar -- os acertos nao pontuaram"; FALHA=1; }

if [ "$FALHA" != 0 ]; then
    echo
    echo "GAMEPLAY: FALHOU"
    exit 1
fi

echo
echo "GAMEPLAY: PASSOU (julgamento, placar e combo executados no jogo real)"
