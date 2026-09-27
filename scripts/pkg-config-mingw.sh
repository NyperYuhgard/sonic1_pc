#!/bin/sh
# pkg-config wrapper for the mingw-w64 cross build of sonic1_pc.
#
# Why this exists: when CMake cross-compiles it overwrites PKG_CONFIG_LIBDIR
# with a path derived from the compiler name, so a PKG_CONFIG_LIBDIR exported
# by the toolchain file is ignored and the host (Linux) .pc files are picked
# instead. Pinning the search path inside the pkg-config executable itself is
# immune to that.
#
# The prefix comes from $SONIC_MINGW_PREFIX (exported by build-windows.sh and
# also passed to CMake as -DSONIC_MINGW_PREFIX). Optional: SONIC_MINGW_SYSROOT
# for staging-dir style layouts that need PKG_CONFIG_SYSROOT_DIR.

prefix="${SONIC_MINGW_PREFIX:-/usr/x86_64-w64-mingw32}"

libdir=""
for d in "$prefix/lib/pkgconfig" "$prefix/lib64/pkgconfig" "$prefix/share/pkgconfig"; do
    [ -d "$d" ] || continue
    if [ -z "$libdir" ]; then
        libdir="$d"
    else
        libdir="$libdir:$d"
    fi
done

if [ -z "$libdir" ]; then
    echo "pkg-config-mingw: no pkgconfig directory under $prefix" >&2
    echo "  set SONIC_MINGW_PREFIX (or pass --prefix) to the mingw-w64 prefix" >&2
    echo "  that contains pkgconfig/sdl2.pc and pkgconfig/SDL2_mixer.pc" >&2
    exit 1
fi

PKG_CONFIG_LIBDIR="$libdir"
export PKG_CONFIG_LIBDIR

# Hide anything inherited from the host environment.
PKG_CONFIG_PATH=""
export PKG_CONFIG_PATH

if [ -n "${SONIC_MINGW_SYSROOT:-}" ]; then
    PKG_CONFIG_SYSROOT_DIR="$SONIC_MINGW_SYSROOT"
    export PKG_CONFIG_SYSROOT_DIR
fi

exec "${SONIC_PKG_CONFIG:-pkg-config}" "$@"
