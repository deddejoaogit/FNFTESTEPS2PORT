#!/bin/sh
# Build the FNF PS2 port in one command.
#
#   ./build.sh              build, then validate the ELF
#   ./build.sh --deploy     build, then stage the memory-card layout
#   ./build.sh --deploy-only stage the memory-card layout from the ELF you have
#   ./build.sh --fast       build with -O1 instead of -O2, to iterate quickly
#   ./build.sh --quick      build only, skip the offline suite and validation
#   ./build.sh --clean      delete every object and start over
#   ./build.sh --tests      run only the offline suite (no PS2 toolchain needed)
#   ./build.sh --check-assets  verify every asset the port reads is present
#   ./build.sh --assets-dir=D  also look for the assets in D
#   ./build.sh --help
#
# The offline suite runs first on purpose: it is pure C plus Python, it needs no
# toolchain, and it catches the mistakes that are expensive to find in a PS2
# build. A chart that does not load or a chart-events regression fails here in
# about ten seconds instead of after a full compile.
#
# The assets are looked for in assets/ first and then in a shared folder, so the
# 65 MB is downloaded once and every version of the port finds it. See
# find_asset_dirs below for the places it looks.
#
# The toolchain is NOT built here. It cannot be: this sandbox has no network
# access to the archives it needs, and the PS2 SDK's own build wants bison, flex,
# m4, texinfo, autoconf, automake, libtool and cmake, none of which can be
# installed. What this script does is find the toolchain you already have, use
# it, and tell you clearly when it is missing.

set -u

PROG=$(basename "$0")
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE" || exit 1

# ---------------------------------------------------------------- arguments

DO_TESTS=1
DO_BUILD=1
DO_DEPLOY_ONLY=0
FAST=0
DO_VALIDATE=1
DO_DEPLOY=0
DO_CLEAN=0
CHECK_ASSETS=0
JOBS=""
TARGET="fnf_ps2_m84.elf"

while [ $# -gt 0 ]; do
    case "$1" in
        --deploy)  DO_DEPLOY=1 ;;
        --deploy-only) DO_DEPLOY=1; DO_BUILD=0; DO_TESTS=0 ;;
        --fast)    FAST=1 ;;
        --quick)   DO_TESTS=0; DO_VALIDATE=0 ;;
        --tests)   DO_BUILD=0; DO_VALIDATE=0 ;;
        --check-assets) DO_BUILD=0; DO_TESTS=0; DO_VALIDATE=0; DO_DEPLOY=0; CHECK_ASSETS=1 ;;
        --clean)   DO_CLEAN=1 ;;
        --no-tests) DO_TESTS=0 ;;
        --assets-dir) shift; FNF_ASSETS="${1:-}" ;;
        --assets-dir=*) FNF_ASSETS="${1#--assets-dir=}" ;;
        --jobs)    shift; JOBS="${1:-}" ;;
        --jobs=*)  JOBS="${1#--jobs=}" ;;
        --elf)     shift; TARGET="${1:-}" ;;
        -h|--help)
            sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "$PROG: unknown option '$1' (try --help)" >&2; exit 2 ;;
    esac
    shift
done

# ------------------------------------------------------------------ output

if [ -t 1 ]; then
    C_RED=$(printf '\033[31m'); C_GRN=$(printf '\033[32m')
    C_YEL=$(printf '\033[33m'); C_BLD=$(printf '\033[1m'); C_OFF=$(printf '\033[0m')
else
    C_RED=""; C_GRN=""; C_YEL=""; C_BLD=""; C_OFF=""
fi

step() { printf '%s==>%s %s\n' "$C_BLD" "$C_OFF" "$1"; }
ok()   { printf '    %sok%s   %s\n' "$C_GRN" "$C_OFF" "$1"; }
warn() { printf '    %swarn%s %s\n' "$C_YEL" "$C_OFF" "$1"; }
die()  { printf '\n%s%s%s\n' "$C_RED" "$1" "$C_OFF" >&2; exit 1; }

# ------------------------------------------------------------ the toolchain

# Where the toolchain might live, in the order to try. The first is the one the
# build has been reproduced with; the rest are the usual places an installer
# puts it, so a fresh machine does not need an environment variable set.
# Two layouts are real, and looking for only one of them reports a perfectly
# good toolchain as missing:
#
#   old    $PS2DEV/bin/ee-gcc   -- wrapper scripts, the classic ps2dev
#   new    $PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc
#
# The second is what ps2sdk's own Makefile.pref asks for -- EE_TOOL_PREFIX is
# mips64r5900el-ps2-elf- and EE_CC is $(EE_TOOL_PREFIX)gcc -- so a tree with
# that compiler and no ee-gcc wrapper is the current install, not a broken
# one. It also wants ps2sdk/samples/Makefile.pref, because that is what the
# project's Makefile includes.
find_toolchain() {
    for d in \
        "${PS2DEV:-}" \
        /usr/local/ps2dev \
        "$HOME/ps2dev" \
        /opt/ps2dev \
        /usr/local/ps2sdk \
        /var/tmp/ps2dev-root/ps2dev
    do
        [ -n "$d" ] || continue
        if [ -x "$d/bin/ee-gcc" ] || [ -x "$d/ee/bin/ee-gcc" ]; then
            printf '%s\n' "$d"; return 0
        fi
        if [ -x "$d/ee/bin/mips64r5900el-ps2-elf-gcc" ] \
           && [ -f "$d/ps2sdk/samples/Makefile.pref" ]; then
            printf '%s\n' "$d"; return 0
        fi
    done
    return 1
}

# Where the 65 MB of runtime assets may already be sitting. They are the same
# for every version of the port -- converted textures and audio, not code -- so
# the point of looking in more than one place is that a download happens once
# instead of once per package. The project's own assets/ is always first: it is
# where a missing file is told to go, and a file there wins over the shared
# copy, so a version can override one asset without touching the others.
#
# The shared folder is called FNFPS2-assets and is looked for next to the
# project, next to the project's parent, and in the usual Downloads. The
# /mnt/c glob covers WSL, where $HOME is a Linux path that has never heard of
# the user's Downloads.
ASSET_DIRS="assets"
find_asset_dirs() {
    ASSET_DIRS="assets"
    for d in \
        "${FNF_ASSETS:-}" \
        "$HERE/../FNFPS2-assets" \
        "$HERE/../../FNFPS2-assets" \
        "${FNF_SRC:+$FNF_SRC/../FNFPS2-assets}" \
        "${FNF_SRC:+$FNF_SRC/../../FNFPS2-assets}" \
        "$HOME/Downloads/FNFPS2-assets" \
        /mnt/c/Users/*/Downloads/FNFPS2-assets
    do
        [ -n "$d" ] || continue
        [ -d "$d" ] || continue
        case " $ASSET_DIRS " in
            *" $d "*) ;;
            *) ASSET_DIRS="$ASSET_DIRS $d" ;;
        esac
    done
}

# Resolved here, and not earlier: a shell function has to be defined before it
# is called, and an earlier version called it from just above the output setup
# -- before this definition, seventy lines further down. The call silently did
# nothing, so --check-assets looked in assets/ alone and reported every shared
# asset as missing.
find_asset_dirs

# ------------------------------------------------------------------- start

START=$(date +%s)
echo
step "FNF PS2 build"
printf '    target %s, %s cores\n' "$TARGET" "$(nproc 2>/dev/null || echo '?')"

# ------------------------------------------------------------- clean first

if [ "$DO_CLEAN" = 1 ]; then
    step "clean"
    make clean >/dev/null 2>&1
    rm -rf build
    ok "objects and build output removed"
fi

# ---------------------------------------------------------------- the assets

# The sixteen assets main.c opens at runtime are not in the repository -- they
# are converted textures and audio totalling about 65 MB, so they live in
# Google Drive. This checks what is on disk against the manifest, so a partial
# copy to the pendrive is caught here instead of on the console.
if [ "$CHECK_ASSETS" = 1 ]; then
    step "checking the assets the port reads at runtime"
    [ -d assets ] || mkdir -p assets
    python3 tools/check_assets.py $ASSET_DIRS --links
    rc=$?
    if [ "$rc" = 0 ]; then
        printf '\n    %sok%s   every asset the port needs is on disk\n\n' "$C_GRN" "$C_OFF"
    else
        printf '\n    Open assets-faltando.html in your browser: it has one link\n'
        printf '    per missing file, so you do not have to copy ids by hand.\n\n'
    fi
    exit "$rc"
fi

# --------------------------------------------------- offline suite, no SDK

if [ "$DO_TESTS" = 1 ]; then
    step "offline suite (no PS2 toolchain needed)"
    if ! command -v gcc >/dev/null 2>&1; then
        warn "no host gcc, skipping the offline suite"
    elif ! command -v python3 >/dev/null 2>&1; then
        warn "no python3, skipping the offline suite"
    else
        T0=$(date +%s)
        if ./test/run_tests.sh >build-tests.log 2>&1; then
            ok "all suites passed ($(($(date +%s) - T0))s)"
            rm -f build-tests.log
        else
            printf '\n'
            # Show the tail of the log, which is where a failure reports itself,
            # rather than making the user scroll the whole thing.
            tail -40 build-tests.log
            # Then keep it. An earlier version deleted the log in this branch,
            # and the .bat names build.log -- which only the PS2 build creates,
            # further down -- so a suite failure pointed the user at a file that
            # was never written. They went looking, found nothing, and had to
            # paste the console by hand. Save it under both names: build.log is
            # the one they are told about, build-tests.log is the honest one.
            cp -a build-tests.log build.log 2>/dev/null || true
            die "the offline suite failed. Fix that first -- a PS2 build would
     only repeat the same mistake more slowly. Full log: build.log"
        fi
    fi
fi

# ------------------------------------------------------------------- build

if [ "$DO_BUILD" = 1 ] && [ "$DO_DEPLOY_ONLY" = 0 ]; then
    PS2DEV=$(find_toolchain) || PS2DEV=""
    if [ -z "$PS2DEV" ]; then
        cat >&2 <<'MSG'

  No PS2 toolchain found.

  Looked in:  $PS2DEV, /var/tmp/ps2dev-root/ps2dev, /usr/local/ps2dev,
              /opt/ps2dev, ~/ps2dev, /usr/local/ps2sdk

  This sandbox cannot build one for you: the PS2 SDK's build needs bison, flex,
  m4, texinfo, autoconf, automake, libtool and cmake, and none of them can be
  installed here. On your Ubuntu box:

      git clone https://github.com/ps2dev/ps2toolchain
      cd ps2toolchain && ./build-all.sh

  then either put it in one of the paths above, or:

      export PS2DEV=/path/to/ps2dev
      ./build.sh

MSG
        exit 1
    fi

    export PS2DEV
    export PS2SDK="$PS2DEV/ps2sdk"
    case ":$PATH:" in
        *":$PS2DEV/bin:"*) ;;
        *) export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2SDK/bin:$PATH" ;;
    esac

    step "building with $(basename "$PS2DEV")"
    [ -x "$PS2DEV/bin/ee-gcc" ] || [ -x "$PS2DEV/ee/bin/ee-gcc" ] \
        || warn "ee-gcc not on PATH; the SDK's Makefile.pref will supply it"

    N=$(nproc 2>/dev/null || echo 1)
    [ -n "$JOBS" ] && N="$JOBS"

    # Everything the build needs that is not a .c file in src/. Checked here,
    # before make, because a missing input does not look like a compile error:
    # make says "No rule to make target 'src/title_packs.o'" and stops, which
    # none of the error patterns below match, so the failure reached the user as
    # an empty error section and a deleted log. That is how a build that was
    # missing six source files reported nothing at all.
    step "checking the build inputs"
    MISSING=""
    for o in $(sed -n 's/^EE_OBJS *= *//p' Makefile); do
        base=${o%.o}
        found=0
        for e in c s S; do
            [ -f "$base.$e" ] && found=1
        done
        [ "$found" = 1 ] || MISSING="$MISSING $base.{c,s,S}"
    done
    # The .S files pull texture packs straight into the ELF with .incbin, so
    # those binaries are build inputs too -- not runtime assets.
    for s in src/*.S; do
        [ -f "$s" ] || continue
        for inc in $(sed -n 's/^[[:space:]]*\.incbin[[:space:]]*"\([^"]*\)".*/\1/p' "$s"); do
            [ -f "$inc" ] || MISSING="$MISSING $inc"
        done
    done
    if [ -n "$MISSING" ]; then
        printf '\n' >&2
        printf '  These files are needed to build, and are not in the package:\n' >&2
        for m in $MISSING; do
            printf '      %s\n' "$m" >&2
        done
        printf '\n' >&2
        printf '  The .lz4f packs live in the same Drive folder as the runtime\n' >&2
        printf '  assets. Run:   ./build.sh --check-assets\n' >&2
        printf '  and it writes assets-faltando.html with one link per file.\n' >&2
        printf '\n' >&2
        printf '  The sources belong in the repository. If one is missing, the\n' >&2
        printf '  checkout is incomplete -- say so and it gets put back.\n' >&2
        exit 1
    fi
    ok "every source and .incbin input is present"

    # FNX_EXTRA is appended after -O2, so a trailing -O1 wins. This is the
    # Makefile's own extension point; overriding EE_CFLAGS on the command line
    # would discard the -D_EE that Makefile.pref composes and the build would
    # break in tamtypes.h, which is exactly what its comment warns about.
    EXTRA=""
    if [ "$FAST" = 1 ]; then
        EXTRA="-O1"
        printf '    %sfast build%s: -O1, no validation, timing on screen\n' "$C_YEL" "$C_OFF"
    fi

    # Capture the log so a failure can be reported as the errors alone. A PS2
    # build with -Wall -Wextra produces hundreds of lines, and the one that
    # matters is easy to lose in them.
    if make -j"$N" $EXTRA "$TARGET" >build.log 2>&1; then
        ok "compiled ($(($(date +%s) - START))s total)"
    else
        printf '\n%s%s%s\n' "$C_RED" "build failed. The errors, and the lines around them:" "$C_OFF" >&2
        # error: lines first, then the few lines before each so the failing
        # source line is visible, then the last of the log for context.
        # "No rule to make target" is in the list on purpose: that is what a
        # missing source file looks like, and it used to match nothing here.
        grep -n -E 'error:|Error [0-9]|undefined reference|No such file|cannot find|No rule to make target' build.log \
            | head -40
        printf '\n%s\n' "--- context ---" >&2
        grep -n -B2 -A2 -E 'error:|No rule to make target' build.log | head -60 >&2
        # If nothing above matched, the log is the only evidence there is, so
        # show its tail rather than an empty section. An earlier version printed
        # the headers and nothing else, which reads as "the build broke for no
        # reason".
        if ! grep -q -E 'error:|Error [0-9]|undefined reference|No such file|cannot find|No rule to make target' build.log; then
            printf '\n%s\n' "--- no error line matched; the last 40 lines of the log ---" >&2
            tail -40 build.log >&2
        fi
        printf '\n%s\n' "--- full log kept at build.log ---" >&2
        die "build failed"
    fi
    rm -f build.log

    [ -f "$TARGET" ] || die "make reported success but $TARGET is not there"

    # ---------------------------------------------------------- validate
    if [ "$DO_VALIDATE" = 1 ]; then
        step "validating $TARGET"
        SIZE=$(wc -c <"$TARGET" | tr -d ' ')
        # The build this port is reproduced with produces 5,089,600 bytes. A
        # large jump means something was linked in that should not be, or a
        # pack stopped being embedded.
        if [ "$SIZE" -lt 1000000 ]; then
            die "$TARGET is only $SIZE bytes; something did not link in"
        fi
        if [ "$SIZE" -gt 20000000 ]; then
            warn "$TARGET is $SIZE bytes, larger than the 5,089,600 B baseline"
        else
            ok "$TARGET is $SIZE bytes (baseline 5,089,600)"
        fi

        if command -v file >/dev/null 2>&1; then
            F=$(file -b "$TARGET")
            case "$F" in
                *ELF*MIPS*|*MIPS*ELF*) ok "it is a MIPS ELF" ;;
                *) die "$TARGET is not a MIPS ELF: $F" ;;
            esac
        else
            warn "no 'file' command, skipping the ELF type check"
        fi

        if command -v ee-objdump >/dev/null 2>&1; then
            if ee-objdump -h "$TARGET" 2>/dev/null | grep -q '\.text'; then
                ok "it has a .text section"
            else
                warn "could not read the section table"
            fi
        fi
    fi
fi

# ------------------------------------------------------------------ deploy

if [ "$DO_DEPLOY" = 1 ]; then
    step "staging the memory-card layout"
    [ -f "$TARGET" ] || die "nothing to deploy: $TARGET does not exist"

    # The port reads everything under mass:/FNFPS2/. The ELF goes at the root of
    # that folder and the assets under assets/, with each song's chart at
    # assets/songs/<id>/<difficulty>.chart -- which is the layout the registry
    # builds, so the packaging and the loader cannot disagree.
    OUT="${FNF_DEPLOY:-build/ps2}"
    rm -rf "$OUT"
    mkdir -p "$OUT/FNFPS2/assets"

    cp "$TARGET" "$OUT/FNFPS2/" || die "could not copy the ELF"
    ok "ELF -> $OUT/FNFPS2/$(basename "$TARGET")"

    # The assets are not in the workspace: they live in Google Drive. Rather
    # than fail, stage what is here and report the gaps against the manifest,
    # so the user knows exactly what to copy across. A wrong size means a
    # truncated download, which is the usual way a pendrive copy goes wrong.
    #
    # Staged from every directory find_asset_dirs found, project first: a file
    # already in assets/ is not overwritten by the shared copy, so one version
    # can override a single asset without forking the whole 65 MB.
    # Staged by the manifest, not by copying the directory: only files the port
    # actually reads are laid out, and a truncated file in assets/ cannot shadow
    # a complete copy in the shared folder, because the size is what decides.
    # Copying the directory used to put .gitkeep and LEIA-ME.txt on the memory
    # card, which is bookkeeping with no business being there.
    if command -v python3 >/dev/null 2>&1; then
        # Word-split on purpose: ASSET_DIRS is a list of directories.
        # shellcheck disable=SC2086
        python3 tools/check_assets.py $ASSET_DIRS --stage-into "$OUT/FNFPS2/assets" || true
    else
        for d in $ASSET_DIRS; do
            [ -d "$d" ] || continue
            cp -rn "$d"/. "$OUT/FNFPS2/assets/" 2>/dev/null
        done
    fi
    ok "assets -> $OUT/FNFPS2/assets/ ($(find "$OUT/FNFPS2/assets" -type f | wc -l | tr -d ' ') files)"
    if command -v python3 >/dev/null 2>&1; then
        # Word-split on purpose: ASSET_DIRS is a list of directories.
        # shellcheck disable=SC2086
        python3 tools/check_assets.py $ASSET_DIRS --missing-only || true
    fi

    # The charts, laid out the way the registry expects them. --deploy-only
    # skips the offline suite, and the suite is what normally fills
    # build/charts; without this, a first --deploy-only silently staged an ELF
    # with no charts at all, which looks like a working deploy right up until
    # the console finds no song.
    if [ ! -d build/charts ] || [ -z "$(ls -A build/charts 2>/dev/null)" ]; then
        if [ -d assets-data/charts ] && command -v python3 >/dev/null 2>&1; then
            step "converting the charts"
            mkdir -p build/charts
            n=0
            for f in assets-data/charts/*-chart.json; do
                [ -e "$f" ] || continue
                sid=$(basename "$f" -chart.json)
                for diff in easy normal hard; do
                    python3 tools/fnf_to_fch3.py \
                        --metadata "assets-data/songs/$sid-metadata.json" \
                        --chart "$f" \
                        --difficulty "$diff" -o "build/charts/$sid-$diff.chart" \
                        >/dev/null 2>&1 && n=$((n + 1))
                done
            done
            if [ "$n" = 0 ]; then
                warn "no chart converted; is assets-data/ present?"
            else
                ok "$n charts converted"
            fi
        fi
    fi

    if [ -d build/charts ]; then
        for f in build/charts/*.chart; do
            [ -e "$f" ] || continue
            b=$(basename "$f" .chart)
            sid=${b%-*}
            diff=${b##*-}
            mkdir -p "$OUT/FNFPS2/assets/songs/$sid"
            cp "$f" "$OUT/FNFPS2/assets/songs/$sid/$diff.chart"
        done
        ok "charts -> $OUT/FNFPS2/assets/songs/ ($(find "$OUT/FNFPS2/assets/songs" -name '*.chart' | wc -l | tr -d ' ') files)"
    fi

    printf '\n    Copy %s/FNFPS2/ to the memory card as mass:/FNFPS2/\n\n' "$OUT"
    printf '    Anything still missing is listed above with its Drive file id;\n'
    printf '    the whole set is in assets-manifest.json.\n\n'
fi

printf '%sdone%s in %ss\n\n' "$C_GRN" "$C_OFF" "$(($(date +%s) - START))"
exit 0
