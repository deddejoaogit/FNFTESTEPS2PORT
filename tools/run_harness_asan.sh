#!/bin/sh
# Harness com ASan + UBSan: mesma coisa que run_harness.sh, mas o binario eh
# instrumentado. O pacote limitado pega estouro DO PACOTE de desenho; estes
# sanitizadores pegam estouro, uso apos free e vazamento em TODOS os outros
# buffers do jogo -- que eh a mesma classe de bug, so que em outro lugar.
#
# Uso: tools/run_harness_asan.sh [quadros] [raiz-dos-assets]
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

FRAMES=${1:-${PS2H_FRAMES:-1400}}
ROOT=${2:-${PS2H_ROOT:-build/harness_assets}}
INPUT=${PS2H_INPUT:-"200:CROSS,420:CROSS,600:CROSS,700:CIRCLE,760:CROSS,860:CIRCLE,920:CROSS,1020:CIRCLE,1080:CROSS,1180:CIRCLE,1240:CROSS,1340:CIRCLE"}

SDK=${PS2SDK_HEADERS:-/tmp/fnf-ps2sdk-headers}
if [ ! -f "$SDK/common/include/tamtypes.h" ]; then
    echo "== baixando os fontes do ps2sdk em $SDK =="
    rm -rf "$SDK"
    git clone --depth 1 -q https://github.com/ps2dev/ps2sdk.git "$SDK" || {
        echo "FALHA: nao consegui obter o ps2sdk. Defina PS2SDK_HEADERS."; exit 1; }
fi

INC=$(find "$SDK" -type d -name include | sed 's|^|-I|' | tr '\n' ' ')
mkdir -p build/ps2sdk_host

# O codigo de draw eh o do ps2sdk, com a UNICA adaptacao mecanica no plzcw.
for f in draw draw2d draw_environment; do
    src="$SDK/ee/draw/src/$f.c"
    dst="build/ps2sdk_host/$f.c"
    if [ ! -f "$dst" ] || [ "$src" -nt "$dst" ]; then
        sed 's/^#ifdef _EE$/#if 0/' "$src" > "$dst"
    fi
done
if ! diff -u "$SDK/ee/draw/src/draw.c" build/ps2sdk_host/draw.c | grep -q '^-[#]ifdef _EE$'; then
    echo "FALHA: a adaptacao do draw.c fugiu do esperado"; exit 1
fi

FLAGS="-O1 -g -std=gnu11 -D_EE -Itools/ps2shim $INC -Isrc -Isrc/engine -Itest/ps2harness \
      -Wall -Wextra -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-unused-parameter -DPS2H=1"

# Os fontes vem do PROPRIO Makefile (EE_OBJS), pelo mesmo motivo do
# run_harness.sh: o console nao liga nesta maquina, entao este link e' a unica
# prova de que o EE_OBJS esta' completo.
OBJ_SRC=$(sed -n 's/^EE_OBJS *= *//p' Makefile | tr ' ' '\n' \
    | grep '^src/' | grep -v '_irx\.o$' \
    | grep -v 'title_packs\.o$' | grep -v 'story_packs\.o$' \
    | sed 's/\.o$/.c/' \
    | tr '\n' ' ')

echo "== compilando o harness com ASan + UBSan =="
# -fno-omit-frame-pointer: sem isso o backtrace do ASan sai truncado e nao da
# para saber onde foi. Custa quase nada e a informacao vale muito.
gcc $FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer \
    $OBJ_SRC \
    src/iomanX_irx.c src/fileXio_irx.c src/usbd_irx.c src/usbhdfsd_irx.c src/audsrv_irx.c \
    src/title_packs.S src/story_packs.S test/ps2harness/ps2h.c \
    build/ps2sdk_host/draw.c build/ps2sdk_host/draw2d.c build/ps2sdk_host/draw_environment.c \
    -o build/fnf_harness_asan -lm -lpthread \
    -Wl,--wrap=draw_rect_filled -Wl,--wrap=draw_rect_textured \
    -Wl,--wrap=draw_setup_environment -Wl,--wrap=draw_texture_transfer \
    2> build/harness_asan_cc.log
rc=$?
if [ $rc -ne 0 ]; then
    echo "FALHOU a compilacao:"
    grep -E "error:" build/harness_asan_cc.log | head -30
    exit 1
fi
echo "  compilou: $(wc -c < build/fnf_harness_asan) bytes, $(grep -c 'warning:' build/harness_asan_cc.log) avisos"

if [ ! -f "$ROOT/FNFPS2/assets/menu_cache.rle" ]; then
    echo "== gerando a raiz de assets =="
    if [ ! -f build/charts/tutorial-easy.chart ]; then
        sh test/run_tests.sh >/dev/null 2>&1 || true
    fi
    python3 tools/gen_harness_assets.py "$ROOT" build/charts || exit 1
fi

echo
echo "== executando $FRAMES quadros com ASan + UBSan =="
# ASan aborta no primeiro erro. Se ele achar algo, o relatorio normal do
# harness nao sai -- e isso eh o que se quer: o erro vem primeiro.
PS2H_FRAMES="$FRAMES" PS2H_ROOT="$ROOT" PS2H_INPUT="$INPUT" \
    PS2H_WATCHDOG="${PS2H_WATCHDOG:-600}" PS2H_STRICT="${PS2H_STRICT:-1}" \
    ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:print_stacktrace=1 \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 \
    timeout -s KILL 900 ./build/fnf_harness_asan
rc=$?
echo
echo "harness-asan terminou com codigo $rc"
[ $rc -eq 0 ]
