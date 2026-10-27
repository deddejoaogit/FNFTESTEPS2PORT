#!/bin/sh
# Build the ZIP the user downloads and double-clicks on Windows.
#
#   tools/empacotar_windows.sh [saida.zip]
#
# O que entra:
#   os fontes, tools/, test/, assets-data/  (2 MB de JSON que a suite le')
#   assets/*.lz4f                           (2,9 MB que o build EMBUTE)
#   COMPILAR.bat + LEIA-ME-COMPILAR.txt
#
# O que NAO entra, e por que:
#   google_drive/  65 MB de tarball do toolchain; o usuario ja tem o dele
#   old_motor/     codigo superado, guardado so como referencia
#   build/         estado de build
#   .git/          nao serve para quem so' quer compilar
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE"

OUT=${1:-/tmp/FNFPS2-windows.zip}
STAGE=$(mktemp -d)
PKG="$STAGE/FNFPS2"

mkdir -p "$PKG"
tar -C "$HERE" -cf - \
    --exclude=build --exclude=old_motor --exclude=google_drive \
    --exclude=.git --exclude=__pycache__ --exclude='*.pyc' \
    --exclude='*.o' --exclude='*.map' --exclude='*.ELF' \
    --exclude='FNFPS2-atualizacao.zip' --exclude='*.log' \
    . | tar -C "$PKG" -xf -

# Os quatro .lz4f sao gitignored, entao o tar acima os pega do disco; se algum
# nao estiver la, o pacote sairia quebrado. Conferir e' mais barato que
# descobrir na casa do usuario.
miss=0
for f in assets/title_logoBumpin.lz4f assets/title_gfDanceTitle.lz4f \
         assets/story_gf_idle.lz4f assets/story_bf_idle.lz4f; do
    if [ ! -f "$PKG/$f" ]; then
        echo "FALTA $f -- o pacote sairia sem um arquivo que o build embute" >&2
        miss=$((miss+1))
    fi
done
[ "$miss" = 0 ] || exit 1

for f in COMPILAR.bat COPIAR-PARA-PENDRIVE.bat LEIA-ME-COMPILAR.txt Makefile build.sh; do
    [ -f "$PKG/$f" ] || { echo "FALTA $f no pacote" >&2; exit 1; }
done

# CRLF no .bat, sempre. Um .bat com LF falha de um jeito que nao tem nada a ver
# com o codigo, e o usuario nao tem como descobrir isso.
python3 - "$PKG/COMPILAR.bat" "$PKG/COPIAR-PARA-PENDRIVE.bat" <<'PY'
import sys
for p in sys.argv[1:]:
    d = open(p, 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    open(p, 'wb').write(d)
    assert d.count(b'\r\n') > 50, p + " sem CRLF"
    assert all(b < 128 for b in d), p + " com byte nao-ASCII"
PY

chmod +x "$PKG"/build.sh "$PKG"/tools/*.sh

rm -f "$OUT"
( cd "$STAGE" && zip -qr -9 "$OUT" FNFPS2 )
rm -rf "$STAGE"

echo "escrito $OUT"
echo "  $(wc -c <"$OUT" | tr -d ' ') bytes"
echo "  $(unzip -l "$OUT" | tail -1 | awk '{print $2}') arquivos"
