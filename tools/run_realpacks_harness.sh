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
    echo "      Rode tools/achatar_tudo.sh e tools/pacotar_palcos.py."
    exit 0
fi
if [ ! -x "$BIN" ]; then
    echo "SKIP: harness nao construido (precisa do clone do ps2sdk)."
    exit 0
fi
if [ ! -d build/harness_assets/FNFPS2/assets ]; then
    echo "SKIP: build/harness_assets nao existe (rode tools/run_harness.sh uma vez)."
    exit 0
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
printf '%-18s %-4s %-10s %-6s %-6s %s\n' MUSICA DIF "POOL LIVRE" PROPS SEM_VRAM PERSONAGENS

FALHAS=0
NS=0
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
        printf '%-18s %-4s %-10s %-6s %-6s %s\n' "$s" "$d" "${LIVRE:-?}" "${PROPS:-?}" "$SEM" "$PERS"
        [ "$SEM" = "0" ] || { echo "  FALHA: $s/$d estourou o pool de VRAM"; FALHAS=$((FALHAS + 1)); }
        [ "${PERS:-0}" -ge 2 ] || { echo "  FALHA: $s/$d carregou menos de 2 personagens"; FALHAS=$((FALHAS + 1)); }
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
echo "PACOTES REAIS: PASSOU ($NS combinacoes com os 46 personagens e os 21 palcos de verdade)"
