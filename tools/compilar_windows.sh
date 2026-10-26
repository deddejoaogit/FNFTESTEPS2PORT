#!/bin/sh
# Build the FNF PS2 port inside WSL and copy the result to the pendrive.
#
# This is the script COMPILAR.bat runs. It is not meant to be typed by hand,
# but it is a normal shell script so it can be.
#
#   tools/compilar_windows.sh [opcoes]
#
#     --pendrive /mnt/f     where the pendrive is mounted (default /mnt/f)
#     --no-pendrive         build only, do not copy anywhere
#     --com-packs           also copy the flattened character/stage packs
#     --version V           stamp the ELF with version V
#     --keep                do not wipe the Linux-side build tree before copying
#
# WHAT IT DOES, IN ORDER
#   1. finds the PS2 toolchain (PS2DEV + PS2SDK) in the usual WSL places;
#   2. copies the tree into the Linux filesystem, because /mnt/* is a network
#      filesystem as far as Linux is concerned and building in place there
#      turns a 30-second build into several minutes;
#   3. runs build.sh --deploy, which runs the offline suite, compiles, and
#      stages the mass:/FNFPS2 layout;
#   4. copies build.log back next to the sources, even on failure, because that
#      is the file to send when something goes wrong;
#   5. copies the staged layout to the pendrive WITHOUT deleting anything
#      already there -- the 16 runtime assets live on the pendrive and are not
#      part of the sources.
#
# The rule this follows: a build tool that guesses is worse than one that
# stops. Every step that can fail says what failed and what to do about it.

set -u

PENDRIVE="/mnt/f"
DO_PENDRIVE=1
WITH_PACKS=0
VERSION=""
KEEP=0

while [ $# -gt 0 ]; do
    case "$1" in
        --pendrive)   shift; PENDRIVE="${1:-/mnt/f}" ;;
        --pendrive=*) PENDRIVE="${1#--pendrive=}" ;;
        --no-pendrive) DO_PENDRIVE=0 ;;
        --com-packs)  WITH_PACKS=1 ;;
        --version)    shift; VERSION="${1:-}" ;;
        --version=*)  VERSION="${1#--version=}" ;;
        --keep)       KEEP=1 ;;
        -h|--help)    sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "opcao desconhecida: $1" >&2; exit 2 ;;
    esac
    shift
done

say() { printf '%s\n' "$*"; }
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
ok()   { printf '   \033[32mok\033[0m  %s\n' "$*"; }
warn() { printf '   \033[33maviso\033[0m  %s\n' "$*"; }
die()  { printf '\n\033[31mFALHOU: %s\033[0m\n' "$*" >&2; exit 1; }

HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE" || exit 1

# ------------------------------------------------------------------ toolchain

step "procurando o toolchain do PS2"

# Every root that a ps2dev install realistically lands in, most likely first.
# The user's own environment variable wins if it is set and real.
CANDIDATES="${PS2DEV:-} /usr/local/ps2dev $HOME/ps2dev /opt/ps2dev /usr/local"
FOUND_ROOT=""
FOUND_PREFIX=""
for root in $CANDIDATES; do
    [ -n "$root" ] || continue
    [ -d "$root" ] || continue
    # Two layouts occur: <root>/ee/bin (the standard ps2dev tree) and
    # <root>/bin (a merged bin directory). Both are checked.
    for pre in "$root" "$root/ps2dev"; do
        if [ -x "$pre/ee/bin/mips64r5900el-ps2-elf-gcc" ] || \
           [ -x "$pre/bin/mips64r5900el-ps2-elf-gcc" ]; then
            FOUND_ROOT="$pre"
            break 2
        fi
    done
done

if [ -z "$FOUND_ROOT" ]; then
    say ""
    say "Nao encontrei mips64r5900el-ps2-elf-gcc em nenhum destes lugares:"
    for root in $CANDIDATES; do [ -n "$root" ] && say "    $root"; done
    say ""
    say "O toolchain do PS2 nao vem instalado por padrao no Ubuntu. Para instalar,"
    say "dentro do WSL:"
    say ""
    say "    sudo apt update"
    say "    sudo apt install -y build-essential git cmake texinfo flex bison \\"
    say "                        libelf-dev libmpc-dev libgmp-dev libmpfr-dev \\"
    say "                        libncurses5-dev gettext autoconf automake libtool"
    say "    git clone https://github.com/ps2dev/ps2dev.git ~/ps2dev-src"
    say "    cd ~/ps2dev-src && ./build-all.sh"
    say ""
    say "Ou, se voce ja tem o toolchain em outro lugar, rode:"
    say "    PS2DEV=/caminho/do/ps2dev tools/compilar_windows.sh"
    exit 3
fi

export PS2DEV="$FOUND_ROOT"
# The SDK is normally PS2DEV/ps2sdk; a merged install may have it elsewhere.
if [ -d "$PS2DEV/ps2sdk" ]; then
    export PS2SDK="$PS2DEV/ps2sdk"
elif [ -d "$PS2DEV/../ps2sdk" ]; then
    export PS2SDK="$(cd "$PS2DEV/../ps2sdk" && pwd)"
else
    die "achei o compilador em $PS2DEV mas nao achei o ps2sdk (esperava $PS2DEV/ps2sdk)"
fi
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin:$PATH"

ok "PS2DEV   $PS2DEV"
ok "PS2SDK   $PS2SDK"
ok "gcc      $("$PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc" -dumpversion 2>/dev/null || \
               "$PS2DEV/bin/mips64r5900el-ps2-elf-gcc" -dumpversion 2>/dev/null)"

# The two packs that the assembler pulls into the ELF with .incbin. They are
# not in Git (they are megabytes of art) and the build cannot start without
# them, so this is checked before anything else.
step "conferindo os arquivos que o build embute no ELF"
MISSING=0
for f in assets/title_logoBumpin.lz4f assets/title_gfDanceTitle.lz4f \
         assets/story_gf_idle.lz4f assets/story_bf_idle.lz4f; do
    if [ -f "$f" ]; then
        ok "$f ($(wc -c <"$f" | tr -d ' ') bytes)"
    else
        warn "FALTA $f"
        MISSING=$((MISSING + 1))
    fi
done
if [ "$MISSING" != 0 ]; then
    say ""
    say "Estes arquivos vem no pacote junto com o COMPILAR.bat (pasta assets/)."
    say "Se voce copiou so os fontes, copie a pasta assets/ inteira."
    die "$MISSING arquivo(s) que o build embute estao faltando"
fi

# --------------------------------------------------------------- build tree

# /mnt/c and /mnt/f are network filesystems to Linux: every stat crosses into
# Windows. Compiling in place there is minutes instead of seconds, so the tree
# is copied to $HOME and built there.
WORK="$HOME/FNFPS2-build"
step "copiando para $WORK"
if [ "$KEEP" = 0 ]; then
    rm -rf "$WORK"
fi
mkdir -p "$WORK" || die "nao consegui criar $WORK"
tar -C "$HERE" -cf - \
    --exclude=build --exclude=.git --exclude=old_motor \
    --exclude=google_drive --exclude='*.o' --exclude='*.map' \
    --exclude=__pycache__ --exclude='*.ELF' . 2>/dev/null | tar -C "$WORK" -xf - \
    || die "nao consegui copiar a arvore do projeto"
ok "arvore copiada"

cd "$WORK" || die "nao consegui entrar em $WORK"

# python3 is needed by the offline suite and the chart converter. It is not a
# hard requirement of the compiler itself, so its absence downgrades to a
# warning plus --no-tests rather than a failure.
BUILD_ARGS="--deploy --elf FNF_PS2.ELF"
if [ -n "$VERSION" ]; then
    BUILD_ARGS="$BUILD_ARGS --version $VERSION"
fi
if ! command -v python3 >/dev/null 2>&1; then
    warn "python3 nao encontrado: pulando a suite offline (--no-tests)"
    BUILD_ARGS="$BUILD_ARGS --no-tests"
fi
# The 16 runtime assets usually live on the pendrive already. Pointing the
# deploy at them means the staged layout is complete and the copy to the
# pendrive changes nothing that is already right.
if [ -d "$PENDRIVE/FNFPS2/assets" ]; then
    BUILD_ARGS="$BUILD_ARGS --assets-dir=$PENDRIVE/FNFPS2/assets"
    ok "assets de runtime encontrados no pendrive, usados como fonte"
fi

step "compilando"
say "    ./build.sh $BUILD_ARGS"
rc=0
# shellcheck disable=SC2086
./build.sh $BUILD_ARGS || rc=$?

# The log has to come back even on failure: it is the file the user is asked
# to send.
for f in build.log build-tests.log assets-faltando.html; do
    [ -f "$f" ] && cp -a "$f" "$HERE/$f" 2>/dev/null || true
done

if [ "$rc" != 0 ]; then
    say ""
    die "o build falhou (codigo $rc). O log completo esta em:"
fi

ELF="build/ps2/FNFPS2/FNF_PS2.ELF"
[ -f "$ELF" ] || die "o build disse que passou mas $ELF nao existe"
ok "ELF gerado: $(wc -c <"$ELF" | tr -d ' ') bytes"

# ------------------------------------------------------------------ pendrive

if [ "$DO_PENDRIVE" = 0 ]; then
    say ""
    say "Build pronto (sem copiar para o pendrive). A pasta para copiar e:"
    say "    $WORK/build/ps2/FNFPS2/"
    exit 0
fi

step "copiando para o pendrive ($PENDRIVE)"

if [ ! -d "$PENDRIVE" ]; then
    say ""
    say "O pendrive nao esta montado em $PENDRIVE dentro do WSL."
    say "No Windows ele pode estar em F:, mas o WSL so ve a unidade se ela"
    say "estiver montada. Verifique com:  ls /mnt/f"
    say ""
    say "Se o pendrive estiver em outra letra, rode:"
    say "    tools/compilar_windows.sh --pendrive /mnt/g"
    say ""
    say "O build esta pronto de qualquer forma, em:"
    say "    $WORK/build/ps2/FNFPS2/"
    exit 4
fi

DEST="$PENDRIVE/FNFPS2"
mkdir -p "$DEST" || die "nao consegui criar $DEST (o pendrive esta protegido contra escrita?)"

# COPY, NEVER DELETE. The runtime assets (65 MB) are on the pendrive and are not
# in the sources; a delete-then-copy would wipe them and the game would boot to
# a black screen with a log full of failed opens. The ELF and the charts are
# overwritten because they are what this build produces.
SRCSTAGE="build/ps2/FNFPS2"

cp -f "$SRCSTAGE/FNF_PS2.ELF" "$DEST/FNF_PS2.ELF" \
    || die "nao consegui gravar o ELF no pendrive"
ok "FNF_PS2.ELF -> $DEST/FNF_PS2.ELF"

if [ -d "$SRCSTAGE/assets" ]; then
    ( cd "$SRCSTAGE/assets" && \
      find . -type f -exec sh -c '
          for f do
              mkdir -p "$0/$(dirname "$f")" && cp -f "$f" "$0/$f"
          done' "$DEST/assets" {} + ) || warn "algum asset nao copiou"
    ok "assets -> $DEST/assets ($(find "$DEST/assets" -type f | wc -l | tr -d ' ') arquivos no pendrive)"
fi

if [ "$WITH_PACKS" = 1 ] && [ -d "$HERE/build/packs" ]; then
    mkdir -p "$DEST/packs"
    cp -rf "$HERE/build/packs/." "$DEST/packs/" || warn "os packs nao copiaram"
    ok "packs -> $DEST/packs"
elif [ -d "$HERE/build/packs" ]; then
    warn "ha packs em build/packs; use --com-packs para leva-los (110 MB e"
    warn "ainda nao sao lidos pelo jogo, entao nao copiei)"
fi

# A quick sanity check on what a console run needs to even reach the menu.
step "conferindo o que ficou no pendrive"
MISS=0
for f in FNF_PS2.ELF; do
    if [ -f "$DEST/$f" ]; then ok "$f presente"; else warn "$f AUSENTE"; MISS=$((MISS+1)); fi
done
for f in font.rgba font.widths freakyMenu_48000.pcm audio_stems.pcm \
         title_alphabet.rgba menu_cache.rle; do
    if [ -f "$DEST/assets/$f" ]; then
        ok "assets/$f"
    else
        warn "assets/$f AUSENTE -- o jogo vai falhar ao abrir este arquivo"
        MISS=$((MISS+1))
    fi
done

say ""
if [ "$MISS" = 0 ]; then
    printf '\033[32mPRONTO.\033[0m Copie nada: ja esta no pendrive em F:\\FNFPS2\\.\n'
else
    printf '\033[33mPRONTO, com %d arquivo(s) faltando.\033[0m\n' "$MISS"
    say "Os que faltam sao assets de runtime, que vem do Drive e nao do build."
    say "O jogo roda mesmo assim, mas as telas que usam esses arquivos ficam pretas."
fi
say ""
say "Agora, no PS2: uLaunchELF -> mass:/FNFPS2/FNF_PS2.ELF"
say ""
say "Se travar, mande estes dois arquivos que o jogo escreve no pendrive:"
say "    mass:/FNFPS2/BOOT.txt         (ate onde o boot chegou)"
say "    mass:/FNFPS2/FNF_COLHEITA.txt (o log por quadro)"
exit 0
