#!/usr/bin/env bash
# shellcheck shell=bash
#
# Shared helpers for the sonic1_pc build scripts:
#
#   scripts/build-linux.sh    native build            (Linux / WSL2)
#   scripts/build-windows.sh  mingw-w64 cross build   (Linux / WSL2) -> sonic1.exe
#
# This file is sourced, never executed on its own.

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$COMMON_DIR/.." && pwd)"

# Asset folders read from disasm/ at runtime. src/data.c resolves them against
# <exe dir>/assets (readlink("/proc/self/exe") + "/assets", or ./assets when
# that fails), so every build directory gets an "assets" folder fed from the
# sources below. Keep this list in sync with the paths in src/data.c.
ASSET_SOURCE_DIRS="_anim _maps artnem artunc collide levels map16 map256 objpos palette sslayout startpos tilemaps"

# Audio is loaded by src/sound.c from ./assets/... (working directory relative)
# and lives in the untracked assets/ folder at the repository root.
AUDIO_SOURCE_DIRS="Music SoundFX"

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------

if [ -t 1 ]; then
    C_RESET=$'\033[0m'
    C_INFO=$'\033[36m'
    C_OK=$'\033[32m'
    C_WARN=$'\033[33m'
    C_ERR=$'\033[31m'
    C_BOLD=$'\033[1m'
else
    C_RESET='' C_INFO='' C_OK='' C_WARN='' C_ERR='' C_BOLD=''
fi

info() { printf '%s==>%s %s\n' "$C_INFO" "$C_RESET" "$*"; }
ok()   { printf '%s[ok]%s %s\n' "$C_OK" "$C_RESET" "$*"; }
warn() { printf '%s[warn]%s %s\n' "$C_WARN" "$C_RESET" "$*" >&2; }
die()  { printf '%s[error]%s %s\n' "$C_ERR" "$C_RESET" "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Variants
#
# A variant maps to a CMake build type. "release" deliberately keeps an empty
# CMAKE_BUILD_TYPE, which is the project default: that hits the else branch of
# CMakeLists.txt:110 and adds -O2 only (no -g, no -DNDEBUG).
# ---------------------------------------------------------------------------

variant_build_type() {
    case "$1" in
        release)        printf '' ;;           # CMakeLists.txt else branch: -O2
        debug)          printf 'Debug' ;;      # -O0 -g -DDEBUG
        relwithdebinfo) printf 'RelWithDebInfo' ;;  # -O2 -g -DNDEBUG
        asan)           printf 'Debug' ;;      # + -DSONIC_SANITIZE=ON
        *)              return 1 ;;
    esac
}

# Variant sets supported per target. ASan is not available for mingw-w64
# cross builds (no libasan in the mingw runtime), so it is Linux/WSL2 only.
linux_variants()   { printf 'release debug relwithdebinfo asan'; }
windows_variants() { printf 'release debug relwithdebinfo'; }

variant_known() {
    local candidate="$1" valid="$2" v
    for v in $valid; do
        [ "$v" = "$candidate" ] && return 0
    done
    return 1
}

# ---------------------------------------------------------------------------
# Command line parsing (shared flags)
#
# Sets: WANT_VARIANTS, OPT_CLEAN, OPT_RUN, OPT_ASSETS, OPT_JOBS, OPT_HELP and
# leaves unknown-for-the-caller options in OPT_EXTRA.
# ---------------------------------------------------------------------------

default_jobs() {
    local n
    n="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
    printf '%s' "$n"
}

parse_common_args() {
    # $1: space separated list of valid variant names
    local valid="$1"
    shift

    OPT_CLEAN=0
    OPT_RUN=0
    OPT_ASSETS=1
    OPT_HELP=0
    OPT_JOBS="$(default_jobs)"
    OPT_VARIANT_ARGS=()
    OPT_EXTRA=()

    while [ $# -gt 0 ]; do
        case "$1" in
            -h|--help)   OPT_HELP=1 ;;
            -c|--clean)  OPT_CLEAN=1 ;;
            -r|--run)    OPT_RUN=1 ;;
            --no-assets) OPT_ASSETS=0 ;;
            -j|--jobs)
                [ $# -ge 2 ] || die "$1 needs a value"
                OPT_JOBS="$2"; shift ;;
            -j*)         OPT_JOBS="${1#-j}" ;;
            --jobs=*)    OPT_JOBS="${1#--jobs=}" ;;
            -*)          OPT_EXTRA+=("$1") ;;
            all)         OPT_VARIANT_ARGS+=("all") ;;
            *)           OPT_VARIANT_ARGS+=("$1") ;;
        esac
        shift
    done

    [ "$OPT_JOBS" -ge 1 ] 2>/dev/null || die "--jobs expects a positive number, got '$OPT_JOBS'"

    # Expand "all" and validate the requested variants.
    local out=() v
    if [ ${#OPT_VARIANT_ARGS[@]} -eq 0 ]; then
        # Default: first valid variant of the list (release).
        for v in $valid; do out+=("$v"); break; done
    else
        for v in "${OPT_VARIANT_ARGS[@]}"; do
            if [ "$v" = all ]; then
                for v in $valid; do out+=("$v"); done
            elif variant_known "$v" "$valid"; then
                out+=("$v")
            else
                die "unknown variant '$v' (valid: $valid, all)"
            fi
        done
    fi
    WANT_VARIANTS="${out[*]}"
}

# ---------------------------------------------------------------------------
# Toolchain checks
# ---------------------------------------------------------------------------

require_cmd() {
    local cmd="$1" hint="${2:-}"
    command -v "$cmd" >/dev/null 2>&1 || die "'$cmd' not found in PATH${hint:+ - $hint}"
}

# The scripts use `cmake -S/-B`, which needs 3.13 or newer.
require_cmake() {
    require_cmd cmake "install it from https://cmake.org/download/ or your package manager"
    local v
    v="$(cmake --version | head -n 1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -n 1)"
    [ -n "$v" ] || die "could not determine the cmake version"
    if [ "$(printf '%s\n%s\n' "$v" 3.13 | sort -V | head -n 1)" != "3.13" ]; then
        die "cmake $v found, but -S/-B needs 3.13 or newer"
    fi
}

# Host SDL2 + SDL2_mixer development packages (native Linux build).
require_host_sdl2() {
    require_cmd pkg-config "install pkg-config"
    if ! pkg-config --exists sdl2; then
        die "SDL2 development files not found (pkg-config sdl2).
  Debian/Ubuntu: sudo apt install libsdl2-dev
  Fedora:        sudo dnf install SDL2-devel
  Arch:          sudo pacman -S sdl2"
    fi
    if ! pkg-config --exists SDL2_mixer; then
        die "SDL2_mixer development files not found (pkg-config SDL2_mixer).
  Debian/Ubuntu: sudo apt install libsdl2-mixer-dev
  Fedora:        sudo dnf install SDL2_mixer-devel
  Arch:          sudo pacman -S sdl2_mixer"
    fi
}

# ---------------------------------------------------------------------------
# Asset staging
# ---------------------------------------------------------------------------

# stage_one <source> <destination> <link|copy>
stage_one() {
    local src="$1" dst="$2" mode="$3"
    [ -e "$src" ] || return 0
    rm -rf "$dst"
    if [ "$mode" = copy ]; then
        mkdir -p "$dst"
        cp -R "$src/." "$dst/"
    else
        ln -s "$src" "$dst"
    fi
}

# stage_assets <build dir> <link|copy> - populates <build dir>/assets.
#   link: symlinks, instant, stays valid while the repo does (native builds)
#   copy: real copy, so the folder can be moved/copied to another machine
# Echoes the number of staged folders.
stage_assets() {
    local build_dir="$1" mode="$2"
    local dest="$build_dir/assets"
    local count=0 d

    mkdir -p "$dest"
    for d in $ASSET_SOURCE_DIRS; do
        if [ ! -d "$REPO_ROOT/disasm/$d" ]; then
            warn "asset folder not found: disasm/$d (the game will fail to load some data)"
            continue
        fi
        stage_one "$REPO_ROOT/disasm/$d" "$dest/$d" "$mode"
        count=$((count + 1))
    done
    for d in $AUDIO_SOURCE_DIRS; do
        if [ ! -d "$REPO_ROOT/assets/$d" ]; then
            warn "audio folder not found: assets/$d (converted .ogg/.wav, not committed)"
            continue
        fi
        stage_one "$REPO_ROOT/assets/$d" "$dest/$d" "$mode"
        count=$((count + 1))
    done
    printf '%s' "$count"
}

# ---------------------------------------------------------------------------
# CMake driver
# ---------------------------------------------------------------------------

# configure_build <source dir> <build dir> <build type> [extra cmake args...]
configure_build() {
    local src="$1" build_dir="$2" build_type="$3"
    shift 3
    info "configure $build_dir (CMAKE_BUILD_TYPE='${build_type}')"
    cmake -S "$src" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" "$@"
}

# build_target <build dir>
build_target() {
    local build_dir="$1"
    info "build $build_dir"
    cmake --build "$build_dir" --parallel "$OPT_JOBS"
}
