#!/bin/sh
# O JOGO RODANDO COM OS PACOTES DE VERDADE (46 personagens, 21 palcos).
#
# Por que existe: tudo o que o harness roda no dia a dia usa os assets
# SINTETICOS de build/harness_assets -- texturas de 64x64, um prop por palco --
# porque a arte do jogo nao esta' no repositorio. Isso prova a logica, mas nao
# prova que o EMPACOTADOR e o LEITOR do jogo combinam: um pacote com quadro
# fora do canvas, stride errado, PSM trocado ou bloco LZ4 truncado passa nos
# testes sinteticos e estoura (ou desenha lixo) so' com o pacote real.
#
# Aqui a raiz de assets e' a mesma do harness, MENOS duas pastas:
#   assets/chars  -> build/packs/chars   (os 46 .chp4/.cfz4 reais)
#   assets/stages -> build/packs/stages  (os 21 palcos reais)
# Tudo por symlink: nao se copia 330 MB por rodada.
#
# O que se cobra:
#   - as 27 musicas x 3 dificuldades carregam, desenham e NAO falham;
#   - nenhum `palco_sem_vram` (o pool de VRAM nao estourou com a arte real);
#   - os personagens CARREGAM (`char_carregado`) e ANIMAM (`char_*_animando`);
#   - sobra de pool por musica, medida e impressa.
#
# Pre-requisito: os pacotes existem (tools/achatar_tudo.sh + pacotar_palcos.py).
# Sem eles o script diz SKIP e sai com 0 -- nao se inventa pacote.
#
# Uso: tools/run_realpacks_harness.sh [quadros]   (padrao 600: 300 quadros ainda
# estao na contagem regressiva, e nenhum personagem dancou)
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-600}
ROOT=build/realpacks
BIN=build/fnf_harness

if [ ! -d build/packs/chars ] || [ ! -d build/packs/stages ]; then
    echo "SKIP: build/packs/chars ou build/packs/stages nao existe."
    echo "      Rode tools/empacotar_reais.sh <clone da arte>, ou"
    echo "      tools/achatar_tudo.sh + tools/pacotar_palcos.py so' para estes dois."
    exit 0
fi
if [ ! -x "$BIN" ]; then
    echo "SKIP: harness nao construido (precisa do clone do ps2sdk)."
    exit 0
fi
# RECOMPILA antes de testar. O binario e' o mesmo do run_harness.sh, e sem esta
# chamada as 81 combinacoes rodavam o jogo de uma build anterior -- o teste
# passava sem ter compilado o codigo novo (aconteceu com o `palco_add`).
echo "== recompilando o harness (para nao testar binario velho) =="
PS2H_BUILD_ONLY=1 sh tools/run_harness.sh >/dev/null 2>&1 || {
    echo "FALHA: nao consegui recompilar o harness"; exit 1; }
if [ ! -d build/harness_assets/FNFPS2/assets ]; then
    echo "SKIP: build/harness_assets nao existe (rode tools/run_harness.sh uma vez)."
    exit 0
fi
# O tools/run_harness.sh RECRIA a raiz de assets a cada execucao e, com isso,
# apaga os stems.pcm das 27 musicas que o tools/preparar_sweep.py gera. Sem eles
# o jogo entra em cada musica, nao acha o audio e fica na contagem -- 78
# combinacoes "nao animaram nenhum personagem". Como este script pode rodar
# depois de qualquer harness, ele mesmo repoe o que falta.
if [ ! -f build/harness_assets/FNFPS2/assets/songs/bopeebo/stems.pcm ]; then
    echo "  (gerando os stems das 27 musicas: tools/preparar_sweep.py)"
    python3 tools/preparar_sweep.py >/dev/null 2>&1 || {
        echo "  FALHA: tools/preparar_sweep.py"; exit 1; }
fi

# ---- a raiz: tudo do harness, menos chars/ e stages/ -----------------------
rm -rf "$ROOT"
mkdir -p "$ROOT/FNFPS2/assets" || exit 1
for f in build/harness_assets/FNFPS2/*; do
    b=$(basename "$f")
    [ "$b" = assets ] && continue
    ln -sfn "$REPO/$f" "$ROOT/FNFPS2/$b"
done
for f in build/harness_assets/FNFPS2/assets/*; do
    b=$(basename "$f")
    case "$b" in chars|stages) continue;; esac
    ln -sfn "$REPO/$f" "$ROOT/FNFPS2/assets/$b"
done
ln -sfn "$REPO/build/packs/chars"  "$ROOT/FNFPS2/assets/chars"
ln -sfn "$REPO/build/packs/stages" "$ROOT/FNFPS2/assets/stages"

LOG="$ROOT/FNFPS2/FNF_COLHEITA.txt"
# O id da MUSICA nao e' o nome da variavel C: `s_philly_nice` tem id
# "philly-nice" (com hifen), e e' o id que vira caminho de arquivo. Tirar do
# nome da variavel fazia 8 das 27 musicas nunca comecarem -- o harness
# "passava" sem entrar no gameplay. O id e' o primeiro campo da struct.
SONGS=$(awk '/^static const sr_song_t s_/ { getline; if (match($0, /"[^"]*"/)) print substr($0, RSTART + 1, RLENGTH - 2) }' \
        src/engine/song_registry_data.inc | tr '\n' ' ')
[ -n "$SONGS" ] || { echo "FALHA: nao consegui ler a lista de musicas do registro"; exit 1; }

echo "== pacotes reais: $(ls build/packs/chars/*.chp4 | wc -l) personagens, $(ls build/packs/stages | wc -l) palcos, $FRAMES quadros por combinacao =="
printf '%-18s %-4s %-10s %-7s %-6s %-6s %s\n' MUSICA DIF "POOL LIVRE" "RAM PALCO" PROPS SEM_VRAM PERSONAGENS

FALHAS=0
NS=0
# A MAIOR RAM DE PALCO VISTA NAS 81 COMBINACOES. O teto e' PALCO_RAM_MAX
# (4 MB de tabela de props no main.c), mas o numero que importa e' o medido: a
# conta do leitor por quadro custa tabela + UM bloco comprimido, nao o pacote
# inteiro -- era isso que deixava o `spookyMansion` com 16 MB no papel
# (tools/check_stage_budget.py, coluna "pacote inteiro") e 462 KB de verdade.
RAMMAX=0
RAMQUEM=""
for s in $SONGS; do
    for d in 0 1 2; do
        rm -f "$LOG"
        if ! FNF_START_SONG=$s FNF_START_DIFF=$d PS2H_ROOT="$ROOT" \
             PS2H_FRAMES="$FRAMES" PS2H_INPUT="0:NONE" PS2H_STRICT=1 \
             timeout 300 "$BIN" >/tmp/realpacks_$$.out 2>&1; then
            echo "  FALHA: $s/$d nao terminou"
            tail -6 /tmp/realpacks_$$.out
            FALHAS=$((FALHAS + 1))
            continue
        fi
        NS=$((NS + 1))
        LIVRE=$(awk -F, '$1=="E" && $3=="palco_pool" {print $4; exit}' "$LOG")
        PROPS=$(awk -F, '$1=="E" && $3=="palco_pool" {print $5; exit}' "$LOG")
        SEM=$(awk -F, '$3=="palco_sem_vram" {n++} END{print n+0}' "$LOG")
        PERS=$(awk -F, '$3=="char_carregado" {n++} END{print n+0}' "$LOG")
        # ULTIMO `palco_ram` da rodada: o valor ACUMULADO com todos os props do
        # palco abertos (o evento sai a cada pacote aberto).
        RAM=$(awk -F, '$1=="E" && $3=="palco_ram" {v=$4} END{print v+0}' "$LOG")
        RMAX=$(awk -F, '$1=="E" && $3=="palco_ram" {v=$5} END{print v+0}' "$LOG")
        printf '%-18s %-4s %-10s %-7s %-6s %-6s %s\n' "$s" "$d" "${LIVRE:-?}" "$RAM" "${PROPS:-?}" "$SEM" "$PERS"
        [ "$SEM" = "0" ] || { echo "  FALHA: $s/$d estourou o pool de VRAM"; FALHAS=$((FALHAS + 1)); }
        [ "${PERS:-0}" -ge 2 ] || { echo "  FALHA: $s/$d carregou menos de 2 personagens"; FALHAS=$((FALHAS + 1)); }
        # A RAM do palco tem de ser MEDIDA e nao pode chegar ao teto: o teto
        # existe para o jogo recusar um prop em vez de estourar a RAM do EE, mas
        # um palco que chega perto dele esta' a um prop de perder cena.
        [ "${RAM:-0}" -gt 0 ] || { echo "  FALHA: $s/$d nao publicou a RAM do palco (palco_ram)"; FALHAS=$((FALHAS + 1)); }
        if [ -n "${RMAX:-}" ] && [ "${RMAX:-0}" -gt 0 ] && [ "${RAM:-0}" -gt "$RMAX" ]; then
            echo "  FALHA: $s/$d cobrou $RAM B de RAM no palco, acima do teto $RMAX"; FALHAS=$((FALHAS + 1)); fi
        if [ "${RAM:-0}" -gt "$RAMMAX" ]; then RAMMAX=${RAM:-0}; RAMQUEM="$s/$d"; fi
        # O evento de animacao sai quando o personagem troca de quadro; ele e' a
        # prova de que o pacote REAL decodificou pixel, e nao so' abriu.
        ANIM=$(awk -F, '$3 ~ /^char_(bf|gf|dad)_animando$/ {n++} END{print n+0}' "$LOG")
        [ "$ANIM" -ge 1 ] || { echo "  FALHA: $s/$d nao animou nenhum personagem"; FALHAS=$((FALHAS + 1)); }
    done
done
rm -f /tmp/realpacks_$$.out

if [ "$FALHAS" != 0 ]; then
    echo
    echo "PACOTES REAIS: FALHOU ($FALHAS combinacao(oes))"
    exit 1
fi

echo
echo "  maior RAM de palco ......... $RAMMAX B ($RAMQUEM) -- teto PALCO_RAM_MAX"
echo
echo "PACOTES REAIS: PASSOU ($NS combinacoes com os 46 personagens e os 21 palcos de verdade)"
