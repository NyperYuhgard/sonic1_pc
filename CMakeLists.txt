cmake_minimum_required(VERSION 3.13)   # target_link_directories()
project(sonic1_pc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find SDL2
find_package(PkgConfig REQUIRED)
#
# SONIC_STATIC_SDL links SDL2, SDL2_mixer and the Ogg/Vorbis codecs statically
# so the Windows build ships a self-contained sonic1.exe with no runtime DLLs.
# It needs the static archives in the cross prefix, and only works with the
# mingw-w64 cross toolchain: -static makes the linker prefer libfoo.a over the
# libfoo.dll.a import libraries, and --allow-multiple-definition papers over
# SDL2's DeleteObject clashing with gdi32's.
option(SONIC_STATIC_SDL "Link SDL2 statically (self-contained .exe, mingw cross only)" OFF)
if(SONIC_STATIC_SDL)
    if(NOT MINGW)
        message(FATAL_ERROR
            "SONIC_STATIC_SDL only supports the mingw-w64 cross build. "
            "Build Windows with scripts/build-windows.sh, which enables it.")
    endif()
    # PKG_CONFIG_ARGN is prepended to every pkg-config call, which is how the
    # --static flag reaches the query (pkg_check_modules has no option for it).
    # Forced on both branches so a static configure never leaks into the next
    # dynamic one, and vice versa.
    set(PKG_CONFIG_ARGN --static CACHE STRING "Extra pkg-config arguments" FORCE)
else()
    set(PKG_CONFIG_ARGN "" CACHE STRING "Extra pkg-config arguments" FORCE)
endif()
pkg_check_modules(SDL2      REQUIRED sdl2)
pkg_check_modules(SDL2_mixer REQUIRED SDL2_mixer)

# Sources
set(SOURCES
    src/main.c
    src/vdp.c
    src/input.c
    src/palette.c
    src/objects.c
    src/sprites.c
    src/sound.c
    src/ram.c
    src/decomp.c
    src/data.c
    src/assets.c
    src/deform.c
    src/level.c
    src/plc.c
    src/hud.c
    src/collision.c
    src/objview.c
    src/freecamera.c
    src/debugmode.c
    src/ramview.c
    src/planeview.c
    src/special.c
    src/enddemo.c
    src/config.c
    src/postprocess.c
    src/tas.c
    src/tas_editor.c
)

# Executable
add_executable(sonic1 ${SOURCES})

target_include_directories(sonic1 PRIVATE
    src
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_mixer_INCLUDE_DIRS}
)

# pkg_check_modules() reports the -L paths separately in *_LIBRARY_DIRS and
# leaves bare names in *_LIBRARIES, so they have to be added as link
# directories. Without this the link only succeeds when the libs sit in the
# linker's default search path, which is true for a host SDL2 in
# /usr/lib/<triple> but not for the mingw cross prefix.
target_link_directories(sonic1 PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_mixer_LIBRARY_DIRS}
)

target_link_libraries(sonic1
    ${SDL2_LIBRARIES}
    ${SDL2_mixer_LIBRARIES}
)

if(SONIC_STATIC_SDL)
    # -mwindows turns off the console subsystem SDL2main otherwise selects.
    target_link_options(sonic1 PRIVATE -mwindows -static -Wl,--allow-multiple-definition)
endif()

target_compile_options(sonic1 PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${SDL2_mixer_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Optional AddressSanitizer + UBSan build, for catching wild memory access
# at the exact source line. Enable with -DSONIC_SANITIZE=ON.
option(SONIC_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
if(SONIC_SANITIZE)
    target_compile_options(sonic1 PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sonic1 PRIVATE -fsanitize=address,undefined)
endif()

# Debug build with -O0 -g
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(sonic1 PRIVATE -O0 -g -DDEBUG)
else()
    target_compile_options(sonic1 PRIVATE -O2)
endif()
