cmake_minimum_required(VERSION 3.10)
project(sonic1_pc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

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
)

# Executable
add_executable(sonic1 ${SOURCES})

target_include_directories(sonic1 PRIVATE
    src
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(sonic1
    ${SDL2_LIBRARIES}
)

target_compile_options(sonic1 PRIVATE
    ${SDL2_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Debug build with -O0 -g
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(sonic1 PRIVATE -O0 -g -DDEBUG)
else()
    target_compile_options(sonic1 PRIVATE -O2)
endif()
