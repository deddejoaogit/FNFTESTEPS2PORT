#!/bin/sh
# Kept because it is the name this project has always built with. Everything it
# used to do -- set PS2DEV/PS2SDK/PATH and run make -- now lives in build.sh,
# which also finds the toolchain, runs the offline suite first, reports build
# failures as the errors alone, validates the ELF and can stage the memory-card
# layout. Run ./build.sh --help for the options.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
exec "$HERE/build.sh" "$@"
