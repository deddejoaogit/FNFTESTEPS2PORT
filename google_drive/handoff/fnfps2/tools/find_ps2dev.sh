#!/bin/sh
# Print the first directory that looks like a ps2dev installation.
#
#   ./find_ps2dev.sh                  -> /usr/local/ps2dev, or nothing
#   PS2DEV=/where ./find_ps2dev.sh
#
# Why a script and not a loop in the .bat: the Windows launcher has to ask the
# shell a question, and piping that through cmd.exe's for /f means escaping the
# shell's own operators (||, $, quotes) for a second parser. Doing it here
# keeps one parser instead of two, and it is testable.
#
# WHAT COUNTS AS AN INSTALL. There are two layouts and both are real:
#
#   old    $PS2DEV/bin/ee-gcc          -- wrapper scripts, the classic ps2dev
#   new    $PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc
#
# The "new" one is what ps2sdk's own Makefile.pref asks for:
#
#     EE_TOOL_PREFIX ?= mips64r5900el-ps2-elf-
#     EE_CC          ?= $(EE_TOOL_PREFIX)gcc
#
# so a tree with the triple-prefixed compiler and no ee-gcc wrapper is not a
# broken install -- it is the current one. Looking only for ee-gcc reports a
# perfectly good toolchain as missing, which is exactly what happened: the
# install had all nine libraries and every header, and the launcher said
# "NAO ACHEI O PS2SDK".
#
# /usr/local/ps2dev comes first because that is where ps2toolchain's
# build-all.sh installs.
EE_CC_NEW=mips64r5900el-ps2-elf-gcc

for p in "${PS2DEV:-}" /usr/local/ps2dev "$HOME/ps2dev" \
         /opt/ps2dev /usr/local/ps2sdk /var/tmp/ps2dev-root/ps2dev; do
    [ -n "$p" ] || continue

    # A wrapper-named compiler is enough on its own.
    if [ -x "$p/bin/ee-gcc" ] || [ -x "$p/ee/bin/ee-gcc" ]; then
        printf '%s\n' "$p"; exit 0
    fi

    # Otherwise ask for the compiler Makefile.pref will actually invoke, and
    # for the SDK makefiles the project includes. One without the other is a
    # partial install, and saying so beats a confusing failure at compile time.
    if [ -x "$p/ee/bin/$EE_CC_NEW" ]; then
        if [ -f "$p/ps2sdk/samples/Makefile.pref" ]; then
            printf '%s\n' "$p"; exit 0
        fi
        echo "$p: tem $EE_CC_NEW mas nao tem ps2sdk/samples/Makefile.pref" >&2
        exit 2
    fi
done
exit 1
