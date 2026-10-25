#!/bin/sh
# Build the package the user downloads and double-clicks.
#
#   tools/empacotar.sh [saida.tar.gz]
#
# What comes out is a single tarball whose root is FNFPS2-<versao>/, holding the
# whole project: build.sh, COMPILAR.bat, the sources, the tests, the tools and
# assets-data (the tests need the chart JSON, so it ships). The 16 runtime
# assets do NOT ship -- they are ~65 MB and live in Drive; --check-assets
# writes an HTML page with a link to each one.
#
# The one thing this script adds beyond `tar czf` is REVISAO.txt: a line naming
# the exact revision inside the tarball. Without it, a report of "the same error
# still happens" cannot distinguish "the fix does not work" from "the old folder
# was extracted over" -- and those need completely different answers. The file
# is generated, never committed, so it cannot go stale against the tree.
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE"

OUT=${1:-/tmp/FNFPS2-$(cat VERSION 2>/dev/null || echo 0.2.105).tar.gz}
VERSION=$(cat VERSION 2>/dev/null || echo 0.2.105)
STAGE=$(mktemp -d)
PKG="$STAGE/FNFPS2-$VERSION"

# Everything in the tree except build state and bytecode. assets-data is 2 MB
# of JSON that the offline suite reads, so it stays.
mkdir -p "$PKG"
tar -C "$HERE" -cf - \
    --exclude=build \
    --exclude=__pycache__ \
    --exclude='*.pyc' \
    --exclude='*.o' \
    --exclude='*.map' \
    --exclude='*.elf' \
    --exclude=REVISAO.txt \
    --exclude=.git \
    --exclude=assets-faltando.html \
    --exclude=build.log \
    --exclude=build-tests.log \
    . | tar -C "$PKG" -xf -

# The revision stamp. Short hash plus date: enough to tell two builds apart,
# and it is what COMPILAR.bat prints on its very first screen.
REV=$(git rev-parse --short HEAD 2>/dev/null || echo desconhecida)
STAMP=$(date -u +%Y-%m-%d)
printf 'FNF PS2 %s, revisao %s (%s)\n' "$VERSION" "$REV" "$STAMP" > "$PKG/REVISAO.txt"

# ------------------------------------------------------------ sanity, in the package
# These run against the staged copy, not the working tree: the point is to
# certify what is about to be zipped, and a stale file left behind in the
# working tree is exactly the kind of thing that must not reach the user.
FAIL=0

# COMPILAR.bat has to be CRLF or cmd.exe misreads the line endings and fails in
# ways that have nothing to do with the code.
if python3 - "$PKG/COMPILAR.bat" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
crlf, lf = d.count(b'\r\n'), d.count(b'\n')
bad = lf - crlf
print('COMPILAR.bat: %d CRLF, %d bare LF' % (crlf, bad))
sys.exit(1 if bad else 0)
PY
then :; else FAIL=1; fi

# The shell scripts must be the opposite: a CRLF in a shebang script makes sh
# look for an interpreter whose name ends in a carriage return. (Checked in
# python, not with grep -q $'\r' -- this script runs under sh, which does not
# understand that quoting and searches for the wrong thing.)
for f in build.sh compilar.sh tools/find_ps2dev.sh tools/wsl-run.sh test/run_tests.sh; do
    if [ ! -f "$PKG/$f" ]; then
        echo "empacotar: falta $f no pacote" >&2; FAIL=1; continue
    fi
    if python3 -c "
import sys
sys.exit(0 if b'\r' in open(sys.argv[1],'rb').read() else 1)" "$PKG/$f"; then
        echo "empacotar: $f tem CR" >&2; FAIL=1
    fi
done

[ -x "$PKG/COMPILAR.bat" ] || [ -f "$PKG/COMPILAR.bat" ] || { echo "empacotar: COMPILAR.bat ausente" >&2; FAIL=1; }
for f in build.sh compilar.sh tools/find_ps2dev.sh tools/wsl-run.sh test/run_tests.sh; do
    [ -x "$PKG/$f" ] || { echo "empacotar: $f nao eh executavel" >&2; FAIL=1; }
done

# The offline suite, in the package. This is the same run the user's first
# screen does, so it has to pass here or it will fail there.
if (cd "$PKG" && ./test/run_tests.sh >/tmp/fnf-pkg-tests.log 2>&1); then
    echo "suite offline no pacote: PASSOU"
else
    echo "empacotar: a suite offline falhou DENTRO do pacote" >&2
    tail -40 /tmp/fnf-pkg-tests.log >&2
    FAIL=1
fi

if [ "$FAIL" != 0 ]; then
    rm -rf "$STAGE"
    echo "empacotar: pacote RECUSADO, nada foi escrito em $OUT" >&2
    exit 1
fi

# The suite above just ran inside $PKG, and run_tests.sh regenerates build/
# there -- the converted charts and the object files. Tarring now would ship
# that, which is how a 340 KB package turned into 730 KB with 179 stray build
# files in it. The staging tar excludes build/, but it had not been created yet
# at that point. Take it back out before zipping.
rm -rf "$PKG/build" "$PKG/__pycache__" "$PKG/tools/__pycache__" "$PKG/test/__pycache__"

rm -f "$OUT"
tar -C "$STAGE" -czf "$OUT" "FNFPS2-$VERSION"
rm -rf "$STAGE"

printf '\n%s\n' "$OUT"
ls -l "$OUT" | awk '{print "tamanho: " $5 " bytes"}'
printf 'conteudo: %s arquivos\n' "$(tar -tzf "$OUT" | grep -vc '/$')"
printf 'revisao:  %s\n' "$(tar -xzf "$OUT" -O "FNFPS2-$VERSION/REVISAO.txt" 2>/dev/null || echo '?')"
