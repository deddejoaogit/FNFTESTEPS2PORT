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
STAGE_OUT=""
ASSETS_FROM=""
WITH_PACKS=0
VERSION=""
KEEP=0

while [ $# -gt 0 ]; do
    case "$1" in
        --pendrive)   shift; PENDRIVE="${1:-/mnt/f}" ;;
        --pendrive=*) PENDRIVE="${1#--pendrive=}" ;;
        --no-pendrive) DO_PENDRIVE=0 ;;
        --assets-from) shift; ASSETS_FROM="${1:-}" ;;
        --assets-from=*) ASSETS_FROM="${1#--assets-from=}" ;;
        --stage-out)   shift; STAGE_OUT="${1:-}" ;;
        --stage-out=*) STAGE_OUT="${1#--stage-out=}" ;;
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

# ------------------------------------------------------------------ pendrive
#
# WHY THIS RUNS BEFORE THE BUILD
#   The runtime assets (font.rgba, menu_cache.rle, the .pcm stems, 65 MB) live
#   only on the pendrive. build.sh is pointed at them with --assets-dir so the
#   staged layout is complete and nothing on the stick is touched. If the wrong
#   directory is assumed here, the build still succeeds but the staged layout is
#   missing those files, and the copy cannot happen at all.
#
# WHAT WENT WRONG IN THE FIELD (2026-10-26)
#   mkdir /mnt/f/FNFPS2 -> "Permission denied", with the stick mounted in
#   Windows. At the same time /mnt/f/FNFPS2/assets did not exist. Two different
#   causes fit those two facts and they need different fixes, so this block
#   prints the evidence instead of picking one:
#     A. the mount is read-only  -> "ro" in /proc/mounts (dirty NTFS volume,
#        BitLocker, or the physical write-protect switch on the stick);
#     B. /mnt/f is not the stick -> no drvfs mount for that path at all, or a
#        different drive letter holds it.
#   A stick that is present and writable is also found automatically: any
#   /mnt/<letter>/FNFPS2 directory counts as the layout this script targets.

# Prints what WSL actually sees about a mount point. Never guesses.
pendrive_report() {
    _p="$1"
    say "    usuario WSL:   $(id -un) (uid $(id -u))"
    say "    permissao:     $(ls -ld "$_p" 2>/dev/null | tr -s ' ')"
    _m=$(grep -E "[[:space:]]$_p[[:space:]]" "${PENDRIVE_MOUNTS:-/proc/mounts}" 2>/dev/null | head -1)
    if [ -n "$_m" ]; then
        say "    montagem:     $_m"
        case "$_m" in
            *[[:space:]]ro[,[:space:]]*|*[[:space:]]ro[[:space:]]*)
                say ""
                say "    CAUSA: a unidade esta montada SOMENTE LEITURA (ro)."
                say "    O WSL faz isso quando o Windows marcou o volume como sujo"
                say "    (desligamento/remocao sem ejetar) ou quando o pendrive tem"
                say "    trava fisica de escrita. Correcao, no Windows: ejetar e"
                say "    reconectar o pendrive, ou rodar 'chkdsk F: /f' e tentar de novo."
                return 1 ;;
        esac
    else
        say "    montagem:     NENHUMA -- $_p nao e' uma unidade do Windows,"
        say "                  e' apenas uma pasta vazia dentro do WSL."
        say ""
        say "    CAUSA: o pendrive nao esta nessa letra. Veja as unidades que o"
        say "    WSL enxerga:"
        _l=$(ls -d /mnt/[a-z] 2>/dev/null | tr '\n' ' ')
        [ -n "$_l" ] || _l="(nenhuma unidade do Windows visivel)"
        say "        $_l"
        return 1
    fi
    if [ ! -w "$_p" ]; then
        say ""
        say "    CAUSA: montada, mas sem permissao de escrita para este usuario."
        _letra=$(printf '%s' "${_p#/mnt/}" | tr 'a-z' 'A-Z')
        say "    Correcao:  sudo mount -t drvfs ${_letra}: $_p -o rw,metadata,uid=$(id -u),gid=$(id -g)"
        return 1
    fi
    return 0
}

step "procurando o pendrive"
if [ "$DO_PENDRIVE" = 1 ]; then
    if [ -d "$PENDRIVE/FNFPS2" ] && [ -w "$PENDRIVE" ]; then
        ok "pendrive em $PENDRIVE (ja tem a pasta FNFPS2)"
    else
        # Exactly one /mnt/<letter>/FNFPS2 counts as the layout we target. Two
        # or more is ambiguous, so it stops and asks rather than picking one.
        # THE SYSTEM DISK IS NEVER THE PENDRIVE.
        # Regression, found on the user's machine (2026-10-26): the scan
        # accepted any /mnt/<letter>/FNFPS2, so a leftover C:\FNFPS2 made it
        # copy the whole build into the Windows system disk and report it as
        # "the pendrive". C: is only scanned if the user names it explicitly
        # with --pendrive. A volume that carries pagefile.sys or a Windows/
        # directory is the system volume whatever letter it has, so it is
        # refused too.
        _hits=""
        for _d in "${MNT_ROOT:-/mnt}"/[d-z]; do
            [ -d "$_d/FNFPS2" ] || continue
            [ -w "$_d" ] || continue
            if [ -f "$_d/pagefile.sys" ] || [ -d "$_d/Windows" ]; then
                warn "$_d tem cara de disco do sistema; ignorado"
                continue
            fi
            _hits="$_hits $_d"
        done
        _n=$(printf '%s' "$_hits" | wc -w | tr -d ' ')
        if [ "$_n" = 1 ]; then
            PENDRIVE=$(printf '%s' "$_hits" | tr -d ' ')
            ok "a letra informada nao serviu; achei a pasta FNFPS2 em $PENDRIVE"
        elif [ "$_n" -gt 1 ]; then
            say ""
            say "Ha mais de um pendrive com a pasta FNFPS2:$_hits"
            say "Escolha um e rode:"
            for _d in $_hits; do say "    tools/compilar_windows.sh --pendrive $_d"; done
            die "pendrive ambiguo"
        else
            warn "nenhuma pasta FNFPS2 encontrada em /mnt/*"
            say "    se o pendrive esta conectado, rode no WSL:  ls /mnt"
            say "    o build continua; a copia sera tentada em $PENDRIVE"
        fi
    fi
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
    --exclude='FNFPS2-atualizacao.zip' --exclude='build.log' \
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
# The 16 runtime assets (font.rgba, menu_cache.rle, the .pcm stems) are not in
# the sources; they exist only as a copy in some FNFPS2/assets directory. If one
# is found, the deploy is pointed at it so the staged layout is COMPLETE and the
# game lands on the stick working. This is a READ, so the Windows system disk is
# fair game here -- unlike the copy, which never targets it.
if [ -z "$ASSETS_FROM" ]; then
    if [ -d "$PENDRIVE/FNFPS2/assets" ]; then
        ASSETS_FROM="$PENDRIVE/FNFPS2/assets"
    else
        for _d in "${MNT_ROOT:-/mnt}"/[a-z]; do
            if [ -d "$_d/FNFPS2/assets" ]; then
                ASSETS_FROM="$_d/FNFPS2/assets"
                break
            fi
        done
    fi
fi
if [ -n "$ASSETS_FROM" ] && [ -d "$ASSETS_FROM" ]; then
    BUILD_ARGS="$BUILD_ARGS --assets-dir=$ASSETS_FROM"
    ok "assets de runtime encontrados em $ASSETS_FROM, usados como fonte"
else
    warn "nenhuma pasta FNFPS2/assets encontrada: o layout sai sem os"
    warn "assets de runtime e as telas que os usam ficam pretas"
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

# The staged mass:/FNFPS2 layout. Defined here because both the export below
# and the pendrive copy use it.
SRCSTAGE="build/ps2/FNFPS2"

# --------------------------------------------------------------- stage out
#
# WHY THIS EXISTS
#   WSL can only write to a Windows drive that Windows itself mounted into it.
#   On the user's machine it could not write F: at all, and the first version of
#   this script then wrote the game to the wrong place. Copying from the Windows
#   side (COMPILAR.bat, robocopy) has no such failure mode: Explorer can always
#   write the stick, so cmd.exe can too.
#   --stage-out exports the complete layout to a directory Windows can see, and
#   the .bat robocopies it into <letra>:\FNFPS2. Nothing is deleted on either
#   side, so assets that only exist on the stick survive.
if [ -n "$STAGE_OUT" ]; then
    step "exportando o jogo completo para $STAGE_OUT"
    rm -rf "$STAGE_OUT" 2>/dev/null
    mkdir -p "$STAGE_OUT" || die "nao consegui criar $STAGE_OUT"
    ( cd "$SRCSTAGE" && tar -cf - . ) | tar -C "$STAGE_OUT" -xf - \
        || die "nao consegui copiar o layout para $STAGE_OUT"
    ok "$(find "$STAGE_OUT" -type f | wc -l | tr -d ' ') arquivos prontos para copiar"
fi

# ------------------------------------------------------------------ pendrive

if [ "$DO_PENDRIVE" = 0 ]; then
    say ""
    say "Build pronto (sem copiar para o pendrive). A pasta para copiar e:"
    say "    $WORK/build/ps2/FNFPS2/"
    [ -n "$STAGE_OUT" ] && say "ja' exportada em $STAGE_OUT"
    exit 0
fi

# The drive letter as Windows writes it, derived from the mount point. Never
# hardcode a letter in a message: the message is how the user checks the result,
# so a wrong letter there is how "it says F: but nothing is on F:" happened.
LETRA=$(printf '%s' "${PENDRIVE#"${MNT_ROOT:-/mnt}"/}" | cut -d/ -f1 | tr 'a-z' 'A-Z')

step "copiando para o pendrive ($PENDRIVE / ${LETRA}:)"

DEST="$PENDRIVE/FNFPS2"

# Packages the staged layout as a ZIP Explorer can extract onto the stick by
# hand. This is the fallback when WSL cannot write the drive: the build has
# already succeeded, so throwing the result away would be the wrong answer.
pendrive_fallback_zip() {
    _zip="$HERE/FNFPS2-atualizacao.zip"
    rm -f "$_zip"
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$SRCSTAGE" "$_zip" <<'PY' || return 1
import os, sys, zipfile
src, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for base, _dirs, files in os.walk(src):
        for f in files:
            p = os.path.join(base, f)
            z.write(p, os.path.relpath(p, src))
PY
        ok "pacote pronto: $HERE/FNFPS2-atualizacao.zip ($(wc -c <"$_zip" | tr -d ' ') bytes)"
        return 0
    fi
    return 1
}

if [ ! -d "$PENDRIVE" ] || [ ! -w "$PENDRIVE" ] || ! mkdir -p "$DEST" 2>/dev/null; then
    say ""
    say "Nao consegui escrever em $DEST."
    say ""
    pendrive_report "$PENDRIVE" || true
    say ""
    say "O BUILD ESTA PRONTO. Nada foi perdido. Para levar para o console,"
    say "escolha uma das duas saidas:"
    say ""
    if pendrive_fallback_zip; then
        say "  1) No Windows, abra a pasta do projeto:"
        say "       $(printf '%s' "$HERE" | sed 's|^/mnt/\([a-z]\)|\U\1:|')  (ou \\\\wsl.localhost\\Ubuntu-22.04$HERE)"
        say "     e extraia FNFPS2-atualizacao.zip DENTRO de ${LETRA}:\\FNFPS2,"
        say "     substituindo os arquivos. NAO apague o resto da pasta:"
        say "     os 16 assets de runtime estao la' e nao vem nos fontes."
    else
        say "  1) Copie a pasta pronta pelo Explorador do Windows:"
        say "       \\\\wsl.localhost\\Ubuntu-22.04$WORK/build/ps2/FNFPS2"
        say "     para  ${LETRA}:\\FNFPS2  (copiar por cima, sem apagar o resto)."
    fi
    say ""
    say "  2) Corrija a montagem e rode o COMPILAR.bat de novo (as mensagens"
    say "     acima dizem exatamente o que corrigir)."
    exit 4
fi

# COPY, NEVER DELETE. The runtime assets (65 MB) are on the pendrive and are not
# in the sources; a delete-then-copy would wipe them and the game would boot to
# a black screen with a log full of failed opens. The ELF and the charts are
# overwritten because they are what this build produces.
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
    printf '\033[32mPRONTO.\033[0m Copie nada: ja esta no pendrive em %s:\\FNFPS2\\.\n' "$LETRA"
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
