#!/bin/sh
# Extrai o toolchain ps2dev minimo do Drive, roda o comando pedido e limpa.
#
# Por que um wrapper: o /tmp do sandbox e podado entre chamadas de ferramenta,
# e o workspace so persiste dentro de /home/user. O tar.gz de 43,8 MB vive em
# google_drive/ (ignorado pelo git e pelo snapshot), mas a extracao sao 223 MB
# e nao podem ficar no snapshot. Entao: extrai em build/toolchain (excluido do
# snapshot e do git), roda, e apaga no fim.
#
# Uso:  sh tools/with_toolchain.sh <comando...>
#   PS2DEV_HARNESS sai exportado apontando para a extracao, que e o que
#   tools/harness_ps2dev.sh consome.

set -e

REPO=$(cd "$(dirname "$0")/.." && pwd)
TAR=""
for c in "$REPO/google_drive/PS2DEV_EE_MIN_FNF.tar.xz" \
         "/home/user/PS2Projects/google_drive/PS2DEV_EE_MIN_FNF.tar.xz"; do
    [ -f "$c" ] && TAR="$c" && break
done
[ -n "$TAR" ] || { echo "with_toolchain: nao achei PS2DEV_EE_MIN_FNF.tar.xz"; exit 1; }

STAGE="$REPO/build/tc-stage"
DEST="$REPO/build/toolchain"
rm -rf "$STAGE" "$DEST"
mkdir -p "$STAGE"
tar -xJf "$TAR" -C "$STAGE"
# o tar tem um unico diretorio de topo; usa o primeiro que aparecer
TOP=$(ls "$STAGE" | head -1)
[ -n "$TOP" ] && [ -d "$STAGE/$TOP" ] || { echo "with_toolchain: extracao vazia"; exit 1; }
mv "$STAGE/$TOP" "$DEST"
rm -rf "$STAGE"
chmod -R u+w "$DEST"

cleanup() { rm -rf "$DEST"; }
trap cleanup EXIT INT TERM

export PS2DEV_HARNESS="$DEST"
export PS2DEV="$DEST"
export PS2SDK="$DEST/ps2sdk"
export PATH="$DEST/ee/bin:$DEST/bin:$PATH"

"$@"
