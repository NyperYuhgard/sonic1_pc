#!/usr/bin/env bash
# shellcheck shell=bash
#
# Windows (x86_64) build script for sonic1_pc, cross-compiled with mingw-w64
# from Linux/WSL2. Produces a self-contained folder with sonic1.exe, the SDL2
# runtime DLLs and the assets.
#
#   scripts/build-windows.sh                 -> dist/win/release
#   scripts/build-windows.sh debug --clean   -> dist/win/debug
#   scripts/build-windows.sh all             -> every variant
#
# The cross SDL2/SDL2_mixer prefix defaults to /usr/x86_64-w64-mingw32 and can
# be overridden with --prefix DIR or SONIC_MINGW_PREFIX=DIR.

set -euo pipefail

# shellcheck source=scripts/common.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

TOOLCHAIN_FILE="$COMMON_DIR/cmake/toolchain-mingw-w64-x86_64.cmake"
DEFAULT_MINGW_PREFIX="/usr/x86_64-w64-mingw32"

OPT_PREFIX="${SONIC_MINGW_PREFIX:-$DEFAULT_MINGW_PREFIX}"
OPT_DLLS=1
# Static by default: a self-contained sonic1.exe with no runtime DLLs.
OPT_STATIC=1

usage() {
    cat <<EOF
${C_BOLD}Usage:${C_RESET} $(basename "$0") [variant|all] [options]

Cross-compiles sonic1_pc to Windows x86_64 with mingw-w64 and packages the
result in dist/win/<variant>/ (sonic1.exe + assets/).

By default SDL2, SDL2_mixer and the Ogg/Vorbis codecs are linked statically
(CMake option SONIC_STATIC_SDL), so the dist holds a single self-contained
sonic1.exe and needs no SDL2 DLLs at runtime.

${C_BOLD}Variants${C_RESET}:
  release         -O2, no -g, no -DNDEBUG  (project default, fastest)
  debug           -O0 -g -DDEBUG
  relwithdebinfo  -O2 -g -DNDEBUG
  all             release debug relwithdebinfo
  (asan is not available here: mingw-w64 ships no AddressSanitizer. Use
   scripts/build-linux.sh asan to hunt wild memory access in WSL2.)

${C_BOLD}Options:${C_RESET}
      --prefix DIR  mingw-w64 prefix with SDL2/SDL2_mixer
                    (default: \$SONIC_MINGW_PREFIX or $DEFAULT_MINGW_PREFIX)
  -c, --clean       remove the CMake build and dist folders first
      --no-assets   skip the asset staging step
      --no-dlls     do not copy SDL2 DLLs into the dist folder
      --dynamic     link SDL2 dynamically instead (dist ships SDL2*.dll).
                    Needs the shared archives and the runtime DLLs in the
                    prefix, which the static build does not install.
  -j, --jobs N      parallel build jobs (default: $(default_jobs))
  -h, --help        show this help

${C_BOLD}Cross dependencies:${C_RESET}
The prefix must provide pkgconfig/sdl2.pc and pkgconfig/SDL2_mixer.pc built
for mingw-w64, plus the static archives libSDL2.a, libSDL2main.a and
libSDL2_mixer.a (with libvorbisfile.a, libvorbis.a and libogg.a) for the
default static build. On Debian/Ubuntu the cross compiler alone is:
  sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64

SDL2 itself has to be cross-built (or taken from a prepared bundle), for
example with:
  cmake -S SDL2 -B build-sdl2 \\
        -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \\
        -DSONIC_MINGW_PREFIX=$HOME/mingw64 \\
        -DCMAKE_INSTALL_PREFIX=$HOME/mingw64 \\
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
  cmake --build build-sdl2 --target install
and the same for SDL2_mixer (-DSDL2MIXER_* options, with
-DSDL2MIXER_VORBIS=VORBISFILE -DSDL2MIXER_DEPS_SHARED=OFF), which also needs
libogg and libvorbis cross-built. Do NOT reuse this repo's toolchain file for
the dependencies: it aborts when the prefix has no pkgconfig dir yet, which is
the state they start from.

${C_BOLD}Examples:${C_RESET}
  $(basename "$0") release --clean
  $(basename "$0") all -j8
  $(basename "$0") debug --prefix $HOME/mingw64
EOF
}

# Locate the cross SDL2 pkgconfig directory inside a prefix.
# Echoes the directory, returns 1 when SDL2 or SDL2_mixer is missing.
find_mingw_pkgconfig() {
    local prefix="$1" d
    for d in "$prefix/lib/pkgconfig" "$prefix/lib64/pkgconfig" "$prefix/share/pkgconfig"; do
        [ -d "$d" ] || continue
        if [ -f "$d/sdl2.pc" ] && ls "$d" 2>/dev/null | grep -qi '^SDL2_mixer\.pc$'; then
            printf '%s' "$d"
            return 0
        fi
    done
    return 1
}

check_mingw_sdl2() {
    local prefix="$1"
    if [ ! -d "$prefix" ]; then
        die "mingw-w64 prefix not found: $prefix
  Pass --prefix DIR or set SONIC_MINGW_PREFIX."
    fi
    if ! find_mingw_pkgconfig "$prefix" >/dev/null; then
        die "SDL2/SDL2_mixer for mingw-w64 not found under $prefix
  Expected $prefix/lib/pkgconfig/sdl2.pc and SDL2_mixer.pc (cross-built).
  See --help for the cross-build recipe, or pass --prefix DIR."
    fi
    # Windows SDL2 ships SDL2main (WinMain wrapper); its absence means the
    # cross build was configured with SDL_MAIN_HANDLED, which also works.
    if ! PKG_CONFIG_LIBDIR="$(find_mingw_pkgconfig "$prefix")" \
         pkg-config --libs sdl2 | grep -q 'SDL2main'; then
        warn "sdl2.pc does not list -lSDL2main; fine only if that SDL2 was built with SDL_MAIN_HANDLED"
    fi
}

# copy_mingw_dlls <prefix> <dist dir> - copies the SDL2 runtime next to the exe.
#
# Both naming conventions are matched: the SDL2 CMake build installs SDL2.dll /
# SDL2_mixer.dll (no "lib" prefix) while autotools/MSYS2 builds install
# libSDL2-2.0.dll. The file name is dictated by the import library (it is baked
# into the PE import table), so it must be copied verbatim and never renamed.
copy_mingw_dlls() {
    local prefix="$1" dest="$2" f base found=0 copied=0
    for f in "$prefix"/bin/libSDL2*.dll "$prefix"/bin/SDL2*.dll \
             "$prefix"/bin/libwinpthread-1.dll; do
        [ -e "$f" ] || continue
        found=$((found + 1))
        base="$(basename "$f")"
        [ -e "$dest/$base" ] && continue   # already staged by an earlier run
        cp "$f" "$dest/"
        ok "copied $base"
        copied=$((copied + 1))
    done
    # Warn on "nothing to stage", not on "nothing new to stage": a rebuild over
    # an existing dist folder copies nothing yet still has a complete runtime.
    if [ "$found" = 0 ]; then
        warn "no SDL2 DLLs found in $prefix/bin; the .exe will need them on PATH (or link SDL2 statically)"
    fi
}

main() {
    # Script specific options are extracted before the shared parser runs:
    # they take a value, which parse_common_args would read as a variant name.
    local args=("$@") filtered=() i=0 n=$#
    while [ $i -lt $n ]; do
        case "${args[$i]}" in
            --prefix)
                [ $((i + 1)) -lt $n ] || die "--prefix needs a value"
                OPT_PREFIX="${args[$((i + 1))]}"; i=$((i + 1)) ;;
            --prefix=*) OPT_PREFIX="${args[$i]#--prefix=}" ;;
            --no-dlls)  OPT_DLLS=0 ;;
            --dynamic)  OPT_STATIC=0 ;;
            *)          filtered+=("${args[$i]}") ;;
        esac
        i=$((i + 1))
    done

    local valid
    valid="$(windows_variants)"
    parse_common_args "$valid" ${filtered[@]+"${filtered[@]}"}

    if [ "$OPT_HELP" = 1 ]; then
        usage
        return 0
    fi
    [ ${#OPT_EXTRA[@]} -eq 0 ] || die "unknown option: ${OPT_EXTRA[0]}"

    require_cmake
    require_cmd pkg-config
    require_cmd x86_64-w64-mingw32-gcc "install gcc-mingw-w64-x86-64 (Debian/Ubuntu)"
    require_cmd x86_64-w64-mingw32-windres "install binutils-mingw-w64-x86-64"

    # Make the prefix absolute. A relative one (e.g. --prefix Lib-Windows/prefix)
    # would otherwise be resolved by CMake against the *build* directory, not
    # the caller's cwd, and the toolchain file would report a missing pkgconfig
    # dir even though the prefix is right there.
    # Assigned to a local first: "VAR=$(false) || die" would blank VAR before
    # the message could report what the caller actually asked for.
    local prefix_abs
    if ! prefix_abs="$(cd -- "$OPT_PREFIX" 2>/dev/null && pwd -P)"; then
        die "mingw-w64 prefix not found: $OPT_PREFIX
  Pass --prefix DIR (absolute or relative to the current directory)."
    fi
    OPT_PREFIX="$prefix_abs"

    check_mingw_sdl2 "$OPT_PREFIX"

    # scripts/pkg-config-mingw.sh reads the prefix from the environment.
    export SONIC_MINGW_PREFIX="$OPT_PREFIX"

    info "mingw-w64 prefix: $OPT_PREFIX"

    local v build_dir dist_dir build_type cmake_args staged dll
    for v in $WANT_VARIANTS; do
        build_dir="$REPO_ROOT/build/win/$v"
        dist_dir="$REPO_ROOT/dist/win/$v"
        build_type="$(variant_build_type "$v")"
        cmake_args=(-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
                    -DSONIC_MINGW_PREFIX="$OPT_PREFIX")

        if [ "$OPT_STATIC" = 1 ]; then
            cmake_args+=(-DSONIC_STATIC_SDL=ON)
        fi

        if [ "$OPT_CLEAN" = 1 ]; then
            rm -rf "$build_dir" "$dist_dir"
        fi

        configure_build "$REPO_ROOT" "$build_dir" "$build_type" "${cmake_args[@]}"
        build_target "$build_dir"

        dll="$build_dir/sonic1.exe"
        [ -f "$dll" ] || die "expected binary not found: $dll"

        # Package: exe + DLLs + assets, ready to copy to a Windows machine.
        # With SONIC_STATIC_SDL the SDL2 runtime is linked in, so there are no
        # DLLs to stage and the dist is just the exe plus the assets.
        mkdir -p "$dist_dir"
        cp "$dll" "$dist_dir/sonic1.exe"
        if [ "$OPT_STATIC" = 0 ] && [ "$OPT_DLLS" = 1 ]; then
            copy_mingw_dlls "$OPT_PREFIX" "$dist_dir"
        fi
        if [ "$OPT_ASSETS" = 1 ]; then
            staged="$(stage_assets "$dist_dir" copy)"
            ok "staged $staged asset folders in $dist_dir/assets"
        else
            warn "asset staging skipped; sonic1.exe will not find its data"
        fi

        ok "$v Windows build ready: $dist_dir/sonic1.exe"
    done

    printf '\n'
    info "copy the folder to Windows and run sonic1.exe from inside it"
    info "  (src/data.c resolves assets next to the .exe, src/sound.c from the working directory)"
}

main "$@"
