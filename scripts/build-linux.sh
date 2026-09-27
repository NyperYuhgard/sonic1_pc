#!/usr/bin/env bash
# shellcheck shell=bash
#
# Native Linux build script for sonic1_pc (also works inside WSL2).
#
#   scripts/build-linux.sh                     -> build/release
#   scripts/build-linux.sh debug               -> build/debug
#   scripts/build-linux.sh all                 -> every variant
#   scripts/build-linux.sh all --clean --run   -> rebuild and launch release
#
# Each build lands in build/<variant>/ with an "assets" folder next to the
# executable, because src/data.c loads assets relative to the executable path.

set -euo pipefail

# shellcheck source=scripts/common.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

usage() {
    cat <<EOF
${C_BOLD}Usage:${C_RESET} $(basename "$0") [variant|all] [options]

${C_BOLD}Variants${C_RESET} (build/<variant>/):
  release         -O2, no -g, no -DNDEBUG  (project default, fastest)
  debug           -O0 -g -DDEBUG           (gdb with types, PC debug views)
  relwithdebinfo  -O2 -g -DNDEBUG          (optimized with symbols, profiling)
  asan            -O0 -g -DDEBUG + ASan/UBSan
  all             release debug relwithdebinfo asan

${C_BOLD}Options:${C_RESET}
  -c, --clean      remove the build directory before configuring
  -r, --run        launch the binary afterwards (single variant only)
      --no-assets  skip the asset staging step
  -j, --jobs N     parallel build jobs (default: $(default_jobs))
  -h, --help       show this help

${C_BOLD}Examples:${C_RESET}
  $(basename "$0") debug --run
  $(basename "$0") all --clean -j8
  $(basename "$0") asan

${C_BOLD}Run:${C_RESET}
  src/sound.c loads audio from ./assets (working directory), so start the game
  from the repository root:
    ./build/release/sonic1
EOF
}

main() {
    local valid
    valid="$(linux_variants)"
    parse_common_args "$valid" "$@"

    if [ "$OPT_HELP" = 1 ]; then
        usage
        return 0
    fi
    [ ${#OPT_EXTRA[@]} -eq 0 ] || die "unknown option: ${OPT_EXTRA[0]}"

    require_cmake
    require_host_sdl2

    local v build_dir build_type cmake_args binary staged
    for v in $WANT_VARIANTS; do
        build_dir="$REPO_ROOT/build/$v"
        build_type="$(variant_build_type "$v")"
        cmake_args=()
        [ "$v" = asan ] && cmake_args+=(-DSONIC_SANITIZE=ON)

        [ "$OPT_CLEAN" = 1 ] && rm -rf "$build_dir"

        configure_build "$REPO_ROOT" "$build_dir" "$build_type" ${cmake_args[@]+"${cmake_args[@]}"}
        build_target "$build_dir"

        binary="$build_dir/sonic1"
        [ -x "$binary" ] || die "expected binary not found: $binary"

        if [ "$OPT_ASSETS" = 1 ]; then
            staged="$(stage_assets "$build_dir" link)"
            ok "staged $staged asset folders in $build_dir/assets (symlinks to disasm/ and assets/)"
        else
            warn "asset staging skipped; the game will not find its data"
        fi

        ok "$v build ready: $binary"
    done

    local last
    last="${WANT_VARIANTS##* }"

    if [ "$OPT_RUN" = 1 ]; then
        set -- $WANT_VARIANTS
        if [ $# -gt 1 ]; then
            warn "--run ignored for multiple variants; running the last one ($last)"
        fi
        info "run $last (working directory: $REPO_ROOT)"
        cd "$REPO_ROOT"
        "./build/$last/sonic1"
    else
        printf '\n'
        info "run it with: cd $REPO_ROOT && ./build/$last/sonic1"
    fi
}

main "$@"
