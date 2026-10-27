#!/bin/sh
# Build the FNF PS2 port inside WSL and get it onto the pendrive.
#
# This is the script COMPILAR.bat runs. It is not meant to be typed by hand,
# but it is a normal shell script so it can be.
#
#   tools/compilar_windows.sh [opcoes]
#
#     --pendrive /mnt/f     force this mount point as the destination
#     --no-pendrive         build only, write nothing outside the build tree
#     --verificar           check the whole environment and stop, no build
#     --com-packs           also copy the flattened character/stage packs
#     --version V           stamp the ELF with version V
#     --stage-out DIR       export the complete mass:/FNFPS2 layout to DIR
#     --assets-from DIR     use DIR as the runtime-asset source
#     --keep                do not wipe the Linux-side build tree before copying
#
# WHERE EACH HALF OF THE JOB RUNS
#   Everything that concerns Linux is done here, in bash: finding the
#   toolchain, checking free space, finding the pendrive, compiling, copying
#   and verifying. Everything that concerns Windows is left to COMPILAR.bat.
#   No PowerShell is called from here and none is needed to build.
#
# WHY THE PENDRIVE IS FOUND HERE AND NOT BY GUESSING A LETTER
#   A drive letter means nothing to Linux. What matters is whether the mount
#   point is a real drvfs mount, whether it is rw, and whether a write actually
#   lands. All three are checked for real: /proc/mounts for the first two and a
#   temp file for the third. A directory that merely exists is not a mount, and
#   that is exactly the trap that made an earlier version write the game into
#   C:\FNFPS2 while reporting success.
#
# THE RULE THIS FOLLOWS
#   A build tool that guesses is worse than one that stops. Every step that can
#   fail says what failed, why, and what to do about it.

set -u

PENDRIVE=""
DO_PENDRIVE=1
WITH_PACKS=0
VERSION=""
KEEP=0
STAGE_OUT=""
ASSETS_FROM=""
VERIFICAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --pendrive)   shift; PENDRIVE="${1:-}" ;;
        --pendrive=*) PENDRIVE="${1#--pendrive=}" ;;
        --no-pendrive) DO_PENDRIVE=0 ;;
        --verificar)  VERIFICAR=1 ;;
        --com-packs)  WITH_PACKS=1 ;;
        --version)    shift; VERSION="${1:-}" ;;
        --version=*)  VERSION="${1#--version=}" ;;
        --stage-out)   shift; STAGE_OUT="${1:-}" ;;
        --stage-out=*) STAGE_OUT="${1#--stage-out=}" ;;
        --assets-from) shift; ASSETS_FROM="${1:-}" ;;
        --assets-from=*) ASSETS_FROM="${1#--assets-from=}" ;;
        --keep)       KEEP=1 ;;
        -h|--help)    sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "opcao desconhecida: $1" >&2; exit 2 ;;
    esac
    shift
done

say()  { printf '%s\n' "$*"; }
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
ok()   { printf '   \033[32mok\033[0m  %s\n' "$*"; }
warn() { printf '   \033[33maviso\033[0m  %s\n' "$*"; }
bad()  { printf '   \033[31mERRO\033[0m  %s\n' "$*"; }
die()  { printf '\n\033[31mFALHOU: %s\033[0m\n' "$*" >&2; exit 1; }

HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE" || exit 1

# The state file COMPILAR.bat reads to know what to do with the copy. One line,
# pipe separated: <modo>|<destino Windows>|<destino Linux ou UNC>.
DESTINO_ARQ="$HERE/.fnf_destino"
rm -f "$DESTINO_ARQ" 2>/dev/null || true

# The distro name is only needed to build a \\wsl.localhost path. WSL exports it.
DISTRO_WSL="${WSL_DISTRO_NAME:-${FNF_DISTRO:-Ubuntu-22.04}}"

# ------------------------------------------------------------------ toolchain

step "procurando o toolchain do PS2"

# Every root that a ps2dev install realistically lands in, most likely first.
# The user's own environment variable wins if it is set and real.
CANDIDATES="${PS2DEV:-} /usr/local/ps2dev $HOME/ps2dev /opt/ps2dev /usr/local"
FOUND_ROOT=""
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

# The packs that the assembler pulls into the ELF with .incbin. They are not in
# Git (they are megabytes of art) and the build cannot start without them, so
# this is checked before anything else.
step "conferindo os arquivos que o build embute no ELF"
MISSING=0
for f in assets/title_logoBumpin.lz4f assets/title_gfDanceTitle.lz4f \
         assets/story_gf_idle.lz4f assets/story_bf_idle.lz4f; do
    if [ -f "$f" ]; then
        ok "$f ($(wc -c <"$f" | tr -d ' ') bytes)"
    else
        bad "FALTA $f"
        MISSING=$((MISSING + 1))
    fi
done
if [ "$MISSING" != 0 ]; then
    say ""
    say "Estes arquivos vem no pacote junto com o COMPILAR.bat (pasta assets/)."
    say "Se voce copiou so os fontes, copie a pasta assets/ inteira."
    exit 3
fi

if command -v python3 >/dev/null 2>&1; then
    ok "python3  $(python3 -c 'import sys;print("%d.%d"%sys.version_info[:2])' 2>/dev/null || echo presente)"
else
    warn "python3 nao encontrado: a suite offline sera pulada (--no-tests)"
fi

# ------------------------------------------------------------------ pendrive
#
# All of this runs on the Linux side. /proc/mounts plus a real write test answer
# every question, and they are the truth about what Linux can and cannot write.
# No PowerShell, no guessed letters.

# FNF_MOUNTS and FNF_MNT_ROOT exist so the detection can be tested without a
# real Windows drive: they default to the real thing and nothing else reads
# them. Proven by the tests in this repo, never by the build.
MOUNTS="${FNF_MOUNTS:-/proc/mounts}"
MNT_ROOT="${FNF_MNT_ROOT:-/mnt}"

mount_line()   { awk -v p="$1" '$2==p {print $0}' "$MOUNTS" | head -1; }
nao_montado()  { [ -z "$(mount_line "$1")" ]; }
opcoes_mount() { mount_line "$1" | awk '{print $4}'; }

pode_escrever() {
    _t="$1/.fnf_escrita_$$"
    if ( : >"$_t" ) 2>/dev/null; then
        rm -f "$_t" 2>/dev/null
        return 0
    fi
    return 1
}

eh_disco_sistema() {
    [ -f "$1/pagefile.sys" ] || [ -f "$1/hiberfil.sys" ] || [ -d "$1/Windows" ]
}

livre_mb() {
    df -Pk "$1" 2>/dev/null | awk 'NR==2 {print int($4/1024)}'
}

# Windows path of a mount point: /mnt/f/FNFPS2 -> F:\FNFPS2
# Built with three printfs instead of one parameter expansion with a default:
# "${_r:-\}" is not portable, dash treats the backslash as escaping the brace
# and the string never closes.
win_path() {
    _p="$1"
    _l=$(printf '%s' "$_p" | sed "s|^$MNT_ROOT/||" | cut -d/ -f1 | tr 'a-z' 'A-Z')
    _r=$(printf '%s' "$_p" | sed "s|^$MNT_ROOT/[A-Za-z]*||" | sed 's|/|\\|g')
    printf '%s:' "$_l"
    if [ -n "$_r" ]; then printf '%s' "$_r"; else printf '\\'; fi
    printf '\n'
}

UNC_STAGE=""
unc_path() {
    _p=$(printf '%s' "$1" | sed 's|/|\\|g')
    printf '\\\\wsl.localhost\\%s%s' "$DISTRO_WSL" "$_p"
}

# One line per candidate mount point, for the report. Says what is wrong in
# terms the user can act on.
listar_mnt() {
    say "    o que o Ubuntu ve em /mnt:"
    _any=0
    for d in "$MNT_ROOT"/[a-z] "$MNT_ROOT"/[A-Z] "$MNT_ROOT"/[a-z][a-z]; do
        [ -d "$d" ] || continue
        _any=1
        if nao_montado "$d"; then
            say "      $d  pasta vazia, NAO e' unidade do Windows"
        else
            _op=$(opcoes_mount "$d")
            if eh_disco_sistema "$d"; then
                say "      $d  montado, disco do sistema Windows -- nunca e' destino"
            else
                case ",$_op," in
                    *,ro,*) say "      $d  montado, SOMENTE LEITURA (ro), nao aceito" ;;
                    *)      say "      $d  montado, rw, gravavel" ;;
                esac
            fi
        fi
    done
    [ "$_any" = 1 ] || say "      (nenhum /mnt/<letra> existe)"
}

# Tries one mount point as the destination. On failure leaves the reason in
# REASON, so the caller can print it instead of "algo deu errado".
REASON=""
tentar() {
    REASON=""
    [ -d "$1" ] || { REASON="nao existe"; return 1; }
    if nao_montado "$1"; then
        REASON="e' so uma pasta vazia: o Windows nao montou essa unidade no Ubuntu"
        return 1
    fi
    case ",$(opcoes_mount "$1")," in
        *,ro,*) REASON="esta montada SOMENTE LEITURA (ro)"; return 1 ;;
    esac
    if eh_disco_sistema "$1"; then
        REASON="e' o disco do sistema Windows (pagefile.sys/Windows/); o jogo nunca vai para la"
        return 1
    fi
    if ! pode_escrever "$1"; then
        REASON="a gravacao de teste falhou (permissao ou volume cheio)"
        return 1
    fi
    return 0
}

# A read-only drvfs mount is usually a volume Windows marked dirty, but it can
# also be a mount option. Remounting rw is tried only when sudo needs no
# password; when it does, the user is told the command instead of being asked
# for a password through a script.
tentar_consertar() {
    case ",$(opcoes_mount "$1")," in
        *,ro,*) ;;
        *) return 1 ;;
    esac
    sudo -n true 2>/dev/null || return 1
    sudo -n mount -o remount,rw "$1" 2>/dev/null || return 1
    tentar "$1"
}

ESCOLHIDO=""
RECUSADOS=""

if [ "$DO_PENDRIVE" = 1 ]; then
    step "procurando o pendrive"

    if [ -n "$PENDRIVE" ]; then
        if tentar "$PENDRIVE"; then
            ESCOLHIDO="$PENDRIVE"
            ok "usando $PENDRIVE (informado em --pendrive)"
        else
            warn "$PENDRIVE recusado: $REASON"
            if tentar_consertar "$PENDRIVE"; then
                ESCOLHIDO="$PENDRIVE"
                ok "$PENDRIVE remontado em rw e aceito"
            fi
        fi
    fi

    if [ -z "$ESCOLHIDO" ]; then
        # Prefer a mount that already carries the FNFPS2 layout, so the runtime
        # assets and the charts keep living in the same folder the game reads.
        # D: to Z: only: C: is the Windows system disk and is never a target.
        _pref=""
        _outros=""
        for d in $(awk -v r="$MNT_ROOT" '$2 ~ "^"r"/[d-z]$" {print $2}' "$MOUNTS" | sort -u); do
            if tentar "$d"; then
                if [ -d "$d/FNFPS2" ]; then _pref="$_pref $d"; else _outros="$_outros $d"; fi
            else
                RECUSADOS="$RECUSADOS      $d -- $REASON
"
            fi
        done
        set -- $_pref $_outros
        if [ $# -gt 0 ]; then
            ESCOLHIDO="$1"
            ok "pendrive em $ESCOLHIDO"
            [ -d "$ESCOLHIDO/FNFPS2" ] && ok "a pasta FNFPS2 ja' existe la' (nada sera apagado)"
        fi
    fi

    if [ -z "$ESCOLHIDO" ]; then
        say ""
        say "Nenhuma unidade gravavel do Windows apareceu dentro do Ubuntu."
        listar_mnt
        if [ -n "$RECUSADOS" ]; then
            say "    recusadas:"
            printf '%s' "$RECUSADOS"
        fi
        say ""
        say "O caminho mais comum: o pendrive foi conectado DEPOIS que o WSL ja'"
        say "estava rodando. O WSL so' monta unidades novas quando ele reinicia."
        say ""
        say "  1) No Windows, feche o WSL:      wsl --shutdown"
        say "  2) Conecte o pendrive."
        say "  3) Rode o COMPILAR.bat de novo."
        say ""
        say "Se mesmo assim nao aparecer, monte a mao dentro do WSL (troque a letra):"
        say "     sudo mkdir -p /mnt/f && sudo mount -t drvfs F: /mnt/f -o rw,metadata"
        say ""
        say "O build nao foi iniciado. Nada foi gravado em lugar nenhum."
        exit 6
    fi

    _mb=$(livre_mb "$ESCOLHIDO")
    if [ -n "$_mb" ] && [ "$_mb" -lt 40 ] 2>/dev/null; then
        bad "o destino tem só ${_mb} MB livres; o jogo precisa de ~20 MB"
        exit 6
    fi
    ok "espaco livre no destino: ${_mb:-?} MB"
fi

# Where the runtime assets (font.rgba, menu_cache.rle, the .pcm stems) come
# from. They are not in the sources. This is a READ, so the Windows system disk
# is fine here -- unlike the copy, which never targets it.
step "procurando os assets de runtime"
if [ -z "$ASSETS_FROM" ]; then
    _cands=""
    [ -n "$ESCOLHIDO" ] && _cands="$ESCOLHIDO/FNFPS2/assets"
    for d in $(awk -v r="$MNT_ROOT" '$2 ~ "^"r"/[a-z]$" {print $2}' "$MOUNTS" | sort -u); do
        _cands="$_cands $d/FNFPS2/assets"
    done
    _cands="$_cands $HERE/../FNFPS2/assets"
    for c in $_cands; do
        if [ -f "$c/font.rgba" ] || [ -f "$c/menu_cache.rle" ]; then
            ASSETS_FROM="$c"
            break
        fi
    done
fi
if [ -n "$ASSETS_FROM" ] && [ -d "$ASSETS_FROM" ]; then
    ok "$ASSETS_FROM"
else
    warn "nenhuma pasta FNFPS2/assets com font.rgba/menu_cache.rle encontrada"
    warn "o layout sai sem os assets de runtime e essas telas ficam pretas"
fi

if [ "$VERIFICAR" = 1 ]; then
    say ""
    say "Verificacao concluida. Nada foi compilado (--verificar)."
    exit 0
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

BUILD_ARGS="--deploy --elf FNF_PS2.ELF"
[ -n "$VERSION" ] && BUILD_ARGS="$BUILD_ARGS --version $VERSION"
[ -n "$ASSETS_FROM" ] && BUILD_ARGS="$BUILD_ARGS --assets-dir=$ASSETS_FROM"
command -v python3 >/dev/null 2>&1 || BUILD_ARGS="$BUILD_ARGS --no-tests"

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

SRCSTAGE="build/ps2/FNFPS2"
[ -f "$SRCSTAGE/FNF_PS2.ELF" ] || die "o build disse que passou mas $SRCSTAGE/FNF_PS2.ELF nao existe"
ok "ELF gerado: $(wc -c <"$SRCSTAGE/FNF_PS2.ELF" | tr -d ' ') bytes"
ok "layout pronto: $(find "$SRCSTAGE" -type f | wc -l | tr -d ' ') arquivos"

# --------------------------------------------------------------- export

# --stage-out is used when the copy has to happen from the Windows side: the
# layout is exported to a directory that Windows can read, and COMPILAR.bat
# robocopies it. Default target is inside the build tree, which is native
# Linux and fast, and reachable from Windows as \\wsl.localhost\...
if [ -z "$STAGE_OUT" ]; then
    STAGE_OUT="$WORK/build-out"
fi

step "exportando o layout completo para $STAGE_OUT"
rm -rf "$STAGE_OUT" 2>/dev/null || true
mkdir -p "$STAGE_OUT" || die "nao consegui criar $STAGE_OUT"
( cd "$SRCSTAGE" && tar -cf - . ) | tar -C "$STAGE_OUT" -xf - \
    || die "nao consegui copiar o layout para $STAGE_OUT"
ok "$(find "$STAGE_OUT" -type f | wc -l | tr -d ' ') arquivos prontos"

# Everything the console needs to boot must be here. Checked before the copy so
# a broken layout is caught while it can still be fixed.
RC_FALHA=0
for f in FNF_PS2.ELF; do
    [ -f "$STAGE_OUT/$f" ] || { bad "faltando $f no layout"; RC_FALHA=1; }
done
N_CHARTS=$(find "$STAGE_OUT/assets/songs" -name '*.chart' 2>/dev/null | wc -l | tr -d ' ')
[ "$N_CHARTS" = 0 ] && { bad "nenhum chart no layout"; RC_FALHA=1; }
[ "$RC_FALHA" = 0 ] || die "o layout exportado esta incompleto"

# --------------------------------------------------------------- copy

if [ "$DO_PENDRIVE" = 0 ]; then
    say ""
    say "Build pronto, sem copiar (--no-pendrive)."
    say "    layout: $STAGE_OUT"
    if [ -n "$STAGE_OUT" ]; then
        printf 'windows|%s\n' "$(unc_path "$STAGE_OUT")" >"$DESTINO_ARQ" 2>/dev/null || true
    fi
    exit 0
fi

DESTDIR="$ESCOLHIDO/FNFPS2"
WIN_DEST=$(win_path "$DESTDIR")

step "copiando para $DESTDIR ($WIN_DEST)"
mkdir -p "$DESTDIR" || die "nao consegui criar $DESTDIR"

# COPY, NEVER DELETE. The runtime assets are on the stick and are not in the
# sources; a delete-then-copy would wipe them and the game would boot to a
# black screen. cp -a merges, so what is already there survives.
cp -a "$STAGE_OUT/." "$DESTDIR/" || die "a copia para $DESTDIR falhou"

# --------------------------------------------------------------- verify
#
# A copy that is not read back is a copy that is only believed to have worked.
# Every file is checked for existence and size, and the ELF is compared byte for
# byte, because the ELF is the one file whose truncation kills the boot.
step "conferindo o que ficou no pendrive"
FALHAS=0
N=0
while IFS= read -r rel; do
    N=$((N + 1))
    a="$STAGE_OUT/$rel"
    b="$DESTDIR/$rel"
    if [ ! -f "$b" ]; then
        bad "faltando $rel"; FALHAS=$((FALHAS + 1))
    elif [ "$(wc -c <"$a")" != "$(wc -c <"$b")" ]; then
        bad "tamanho diferente em $rel"; FALHAS=$((FALHAS + 1))
    fi
done <<EOF
$(cd "$STAGE_OUT" && find . -type f | sed 's|^\./||')
EOF

if cmp -s "$STAGE_OUT/FNF_PS2.ELF" "$DESTDIR/FNF_PS2.ELF"; then
    ok "FNF_PS2.ELF identico ao compilado ($(wc -c <"$DESTDIR/FNF_PS2.ELF" | tr -d ' ') bytes)"
else
    bad "FNF_PS2.ELF no pendrive difere do compilado"; FALHAS=$((FALHAS + 1))
fi
ok "$N arquivos conferidos, $FALHAS com problema"
[ "$FALHAS" = 0 ] || die "a copia terminou com $FALHAS arquivo(s) errado(s)"

printf 'linux|%s\n' "$WIN_DEST" >"$DESTINO_ARQ" 2>/dev/null || true

say ""
printf '\033[32mPRONTO.\033[0m O jogo completo esta em %s\n' "$WIN_DEST"
say ""
say "No PS2: uLaunchELF -> mass:/FNFPS2/FNF_PS2.ELF"
say ""
say "O boot e' registrado no proprio pendrive:"
say "    mass:/FNFPS2/BOOT.txt         ate onde o boot chegou"
say "    mass:/FNFPS2/FNF_COLHEITA.txt o log quadro a quadro"
exit 0
