#!/bin/sh
# Build reproduzivel do FNF PS2. Toolchain ps2dev v2.0.0
# sha256 c8e5dedccf62084d476894e88cd0451b57f1ed288e741eb3c1263010aaffc028
set -e
export PS2DEV="${PS2DEV:-/var/tmp/ps2dev-root/ps2dev}"
export PS2SDK="$PS2DEV/ps2sdk"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2SDK/bin:$PATH"
cd "$(dirname "$0")"
make "$@"
sha256sum *.elf
