#!/bin/sh
# Build inside the WSL filesystem, then copy the result back to Windows.
#
#   wsl-run.sh <src> <ps2dev> [build.sh args...]
#
# /mnt/c is a network filesystem as far as Linux is concerned: every stat,
# every open, every write crosses into Windows and back. A PS2 build touches a
# few hundred files several times over, so building in place there is minutes
# instead of seconds -- and it gets worse as the tree grows. So copy the tree
# into the Linux filesystem, build there, and copy back only what the user
# needs: the staged memory-card layout, the charts, and build.log.
#
# The copy is not free, but it is linear: a few seconds for a few tens of
# megabytes, against minutes saved on every compile.
set -e

SRC=$1; shift
PS2DEV=$1; shift

[ -n "$SRC" ] || { echo "wsl-run.sh: missing source directory" >&2; exit 2; }
[ -d "$SRC" ] || { echo "wsl-run.sh: $SRC is not a directory" >&2; exit 2; }

export PS2DEV
# The build runs in a copy, under $HOME, so a shared assets folder that sits
# next to the original tree on the Windows side is not a sibling of $WORK. Pass
# the original path so find_asset_dirs can look next to it too -- without this,
# --assets-dir and the sibling search only ever find folders that happen to be
# reachable from $HOME.
export FNF_SRC="$SRC"
WORK="$HOME/FNFPS2-build"

rm -rf "$WORK"
mkdir -p "$WORK"
# An ELF in the source tree is deliberate -- --deploy-only is exactly the case
# of shipping an ELF you already have -- so it is carried across. Only stale
# build state is left behind.
tar -C "$SRC" -cf - --exclude=build --exclude='*.o' \
    --exclude='*.map' --exclude=__pycache__ . | tar -C "$WORK" -xf -

cd "$WORK"

# build.log has to come back even on failure, because that is the file the user
# is asked to send. Copy it out before deciding what to say.
rc=0
./build.sh "$@" || rc=$?

if [ -f build.log ]; then
    cp -a build.log "$SRC/build.log" || true
fi
if [ -f build-tests.log ]; then
    cp -a build-tests.log "$SRC/build-tests.log" || true
fi
if [ -f assets-faltando.html ]; then
    cp -a assets-faltando.html "$SRC/assets-faltando.html" || true
fi

if [ "$rc" != 0 ]; then
    echo
    echo "O build falhou. O log completo esta' em:"
    echo "    $SRC/build.log"
    exit "$rc"
fi

# Only the deploy output is worth carrying back; the object files and the
# intermediate build tree stay here where they are cheap to make again.
mkdir -p "$SRC/build"
rm -rf "$SRC/build/ps2" "$SRC/build/charts"
for d in ps2 charts; do
    if [ -d "build/$d" ]; then
        cp -a "build/$d" "$SRC/build/$d"
    fi
done

echo
echo "Pronto. Saida em: $SRC/build/ps2/FNFPS2/"
exit 0
