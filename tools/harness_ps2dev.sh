#!/bin/sh
# Monta o toolchain ps2dev a partir de google_drive/tree/ e compila o ELF com os
# flags exatos do Makefile.eeglobal do ps2sdk. Veja HARNESS_TOOLCHAIN.md para o
# porquê de cada flag e para a armadilha dos nomes repetidos de header.
#
# Uso: tools/harness_ps2dev.sh [versao] [elf-de-saida] [dir-de-objetos]

set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
SRC="$REPO"
[ -f "$SRC/src/main.c" ] || SRC="$REPO/fnfps2"
DL="$SRC/google_drive/tree"
[ -d "$DL" ] || DL="$REPO/google_drive/tree"

R=${PS2DEV_HARNESS:-/tmp/ps2dev}
# PS2DEV_HARNESS pode apontar para uma extracao do toolchain fora do repo: o
# snapshot do workspace poda os binarios de google_drive/tree. Se ela tiver um
# gcc, ela e a origem; senao continua valendo so como destino.
if [ -n "${PS2DEV_HARNESS:-}" ] && [ -d "$PS2DEV_HARNESS" ] && \
   { [ -f "$PS2DEV_HARNESS/bin/mips64r5900el-ps2-elf-gcc" ] || \
     [ -f "$PS2DEV_HARNESS/ee/bin/mips64r5900el-ps2-elf-gcc" ]; }; then
    DL="$PS2DEV_HARNESS"
fi
VER=${1:-0.2.105-aceito-exato}
OUT=${2:-/tmp/fnf_ps2_m84.elf}
O=${3:-/tmp/harness-objs}

# Dois layouts de $PS2DEV sao aceitos:
#   1. o completo, com o gcc em bin/  (ps2dev-complete-ubuntu22.tar.gz);
#   2. o minimo EE, com o gcc em ee/bin (PS2DEV_EE_MIN_FNF.tar.xz).
# O minimo e o que da para baixar: o completo tem 266 MB e o conector do
# Drive recusa acima de 100 MB. O minimo tem 43,8 MB e ja foi validado
# compilando o projeto inteiro em ambiente isolado.
GCC_REL=""
for c in bin/mips64r5900el-ps2-elf-gcc ee/bin/mips64r5900el-ps2-elf-gcc; do
    [ -f "$DL/$c" ] && GCC_REL="$c" && break
done
if [ -z "$GCC_REL" ]; then
    echo "FALTA o toolchain em $DL"
    echo "Baixe PS2DEV_EE_MIN_FNF.tar.xz (43,8 MB) do Drive e extraia em"
    echo "google_drive/tree/, ou aponte PS2DEV_HARNESS para a extracao."
    echo "Veja HARNESS_TOOLCHAIN.md."
    exit 1
fi

# A arvore de destino reproduz o layout do $PS2DEV. cp -r de uma vez: os
# subdiretorios sao o que impede os headers homonimos de se sobrescrever.
# Se a origem JA e o destino (PS2DEV_HARNESS aponta para a extracao), nao
# copia: copiar para cima de si mesmo apagaria a arvore inteira.
if [ "$DL" != "$R" ]; then
    rm -rf "$R"
    mkdir -p "$(dirname "$R")"
    cp -r "$DL" "$R"
fi
chmod -R u+w "$R" 2>/dev/null || true
# o layout minimo tem o gcc em ee/bin; cria links em $R/bin para os dois
# layouts ficarem iguais daqui para a frente.
if [ -d "$R/ee/bin" ]; then
    mkdir -p "$R/bin"
    for n in mips64r5900el-ps2-elf-gcc mips64r5900el-ps2-elf-gcc-15.2.0 \
             mips64r5900el-ps2-elf-cpp mips64r5900el-ps2-elf-as \
             mips64r5900el-ps2-elf-ld mips64r5900el-ps2-elf-ar \
             mips64r5900el-ps2-elf-nm mips64r5900el-ps2-elf-strip \
             mips64r5900el-ps2-elf-objcopy mips64r5900el-ps2-elf-ranlib \
             mips64r5900el-ps2-elf-readelf mips64r5900el-ps2-elf-size \
             mips64r5900el-ps2-elf-strings mips64r5900el-ps2-elf-objdump \
             mips64r5900el-ps2-elf-c++filt; do
        ln -sf "../ee/bin/$n" "$R/bin/$n" 2>/dev/null || true
    done
fi
G="$R/libexec/gcc/mips64r5900el-ps2-elf/15.2.0"
GG="$R/lib/gcc/mips64r5900el-ps2-elf/15.2.0"
chmod +x "$R"/bin/* "$R"/ee/bin/* "$G"/cc1 "$G"/lto1 "$G"/lto-wrapper 2>/dev/null

# Sem prefixo: o driver procura "as"/"ld" nus, e sem estes links pega o
# assembler x86 do host, que rejeita -EL.
( cd "$R/bin" && for n in as ld ar nm strip objcopy ranlib readelf size strings objdump c++filt; do
    ln -sf "mips64r5900el-ps2-elf-$n" "$n"; done )

cd "$SRC" || exit 1
export PATH="$R/bin:$PATH"

# Flags: Makefile.eeglobal define EE_CFLAGS e a regra de link. O projeto acrescenta
# -Isrc -Isrc/engine e o -DFNF_PS2_VERSION.
# EE_DBGINFOFLAGS do Makefile.eeglobal: -gdwarf-2 -gz. Sem eles o ELF sai
# ~300 KB menor e NAO e o binario que o console espera.
CF="-D_EE -G0 -O2 -Wall -Wextra -Werror=implicit-function-declaration -std=gnu11 -gdwarf-2 -gz"
CF="$CF -I$R/ps2sdk/ee/include -I$R/ps2sdk/common/include -I. -Isrc -Isrc/engine"

rm -rf "$O"; mkdir -p "$O"
fail=0
for f in src/*.c src/engine/*.c src/*.S; do
    [ -f "$f" ] || continue
    o="$O/$(basename "$f" | sed 's/\.[cS]$/.o/')"
    "$R/bin/mips64r5900el-ps2-elf-gcc" $CF \
        -DFNF_PS2_VERSION="\"$VER\"" -c "$f" -o "$o" 2>&1 | grep -E "error:"
    [ -f "$o" ] || { echo "FALHOU: $f"; fail=1; }
done
[ "$fail" = 0 ] || exit 1
echo "objetos: $(ls "$O"/*.o | wc -l)  bytes: $(cat "$O"/*.o | wc -c)"

# O link: -T com o linkfile do ps2sdk e -zmax-page-size=128. Sem os dois o ELF
# sai com o layout default do ld, que é menor e nao é o que o console espera.
"$R/bin/mips64r5900el-ps2-elf-gcc" -T"$R/ps2sdk/ee/startup/linkfile" -O2 \
    -o "$OUT" "$O"/*.o \
    -L"$R/ps2sdk/ee/lib" -L"$R/ee/mips64r5900el-ps2-elf/lib" \
    -L"$R/mips64r5900el-ps2-elf/lib" -Wl,-zmax-page-size=128 \
    -ldraw -lgraph -lpacket -ldma -lmath3d -lpad -laudsrv -lfileXio \
    -lpatches -lkernel -lcglue -lcdvd -lpthreadglue \
    -Wl,-Map,"${OUT%.elf}.map" 2>&1 | grep -E "error|undefined|cannot find"

[ -f "$OUT" ] || { echo "LINK FALHOU"; exit 1; }
echo "ELF: $OUT  $(wc -c < "$OUT") bytes"
