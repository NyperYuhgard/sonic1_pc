# CMake toolchain: cross-compile sonic1_pc to Windows x86_64 with mingw-w64.
#
# Used by scripts/build-windows.sh from Linux/WSL2:
#
#   cmake -S . -B build/win/release \
#         -DCMAKE_TOOLCHAIN_FILE=scripts/cmake/toolchain-mingw-w64-x86_64.cmake
#
# The cross SDL2/SDL2_mixer prefix is taken from SONIC_MINGW_PREFIX (or the
# SONIC_MINGW_PREFIX environment variable used by the script). It must contain
# pkgconfig/sdl2.pc and pkgconfig/SDL2_mixer.pc built for the mingw target.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# --- mingw-w64 toolchain ---------------------------------------------------

set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_AR           x86_64-w64-mingw32-ar      CACHE FILEPATH "")
set(CMAKE_RANLIB       x86_64-w64-mingw32-ranlib  CACHE FILEPATH "")
set(CMAKE_STRIP        x86_64-w64-mingw32-strip   CACHE FILEPATH "")

# The game is plain C, so no C++/Fortran compiler is required.

# Keep the produced .exe free of the libgcc runtime DLL when possible.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc")

# --- Cross prefix ----------------------------------------------------------

if(NOT DEFINED SONIC_MINGW_PREFIX)
    if(DEFINED ENV{SONIC_MINGW_PREFIX})
        set(SONIC_MINGW_PREFIX "$ENV{SONIC_MINGW_PREFIX}")
    else()
        set(SONIC_MINGW_PREFIX "/usr/x86_64-w64-mingw32")
    endif()
endif()
set(SONIC_MINGW_PREFIX "${SONIC_MINGW_PREFIX}" CACHE PATH "mingw-w64 prefix holding SDL2/SDL2_mixer")

set(CMAKE_FIND_ROOT_PATH "${SONIC_MINGW_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# --- pkg-config ------------------------------------------------------------
#
# CMakeLists.txt resolves SDL2 through pkg_check_modules(). Setting
# PKG_CONFIG_LIBDIR here is not enough: when CMake cross-compiles it overwrites
# that variable with a path derived from the compiler name, which makes
# pkg-config fall back to the host (Linux) .pc files. scripts/pkg-config-mingw.sh
# pins the search path inside the pkg-config executable instead and is immune
# to that. It reads the prefix from SONIC_MINGW_PREFIX, which also has to be
# exported in the environment (build-windows.sh does it).

set(SONIC_MINGW_PC_DIRS "")
foreach(_dir lib/pkgconfig lib64/pkgconfig share/pkgconfig)
    if(IS_DIRECTORY "${SONIC_MINGW_PREFIX}/${_dir}")
        list(APPEND SONIC_MINGW_PC_DIRS "${SONIC_MINGW_PREFIX}/${_dir}")
    endif()
endforeach()

if(NOT SONIC_MINGW_PC_DIRS)
    message(FATAL_ERROR
        "No pkgconfig directory under SONIC_MINGW_PREFIX='${SONIC_MINGW_PREFIX}'.\n"
        "Expected ${SONIC_MINGW_PREFIX}/lib/pkgconfig/sdl2.pc and SDL2_mixer.pc "
        "(SDL2 and SDL2_mixer cross-built for mingw-w64).\n"
        "Point SONIC_MINGW_PREFIX at that prefix, e.g. "
        "-DSONIC_MINGW_PREFIX=$HOME/mingw64.")
endif()

set(PKG_CONFIG_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/../pkg-config-mingw.sh"
    CACHE FILEPATH "pkg-config pinned to the mingw-w64 prefix")

# --- Windows specifics -----------------------------------------------------

# assets_base_path() in src/data.c uses readlink("/proc/self/exe") and falls
# back to "./assets" when that fails, which is what happens on Windows. The
# build scripts stage <dist>/assets next to the .exe so both paths resolve.
