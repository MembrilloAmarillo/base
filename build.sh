#!/usr/bin/env bash
set -euo pipefail

# Run from the repository root, regardless of the caller's working directory.
# Override CC, CXX, GLSLC_PATH or GRAPHICS_RELEASE in the environment.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

CC=${CC:-cc}
CXX=${CXX:-c++}
GLSLC_PATH=${GLSLC_PATH:-glslc}
graphics_opt=-O0
if [[ ${GRAPHICS_RELEASE:-0} == 1 ]]; then
    graphics_opt=-O2
fi
graphics_defines=()
pkg_cflags=()
pkg_libs=()
graphics_sources=(graphics graphics_memory graphics_upload graphics_lifetime graphics_command graphics_pipeline graphics_resources graphics_swapchain)
graphics_objects=()
for source in "${graphics_sources[@]}"; do
    graphics_objects+=("$source.o")
done

usage() {
    printf 'Usage: %s [all|debug|release|shaders|clean|vulkan_cube_test|graphics_api_test|graphics_resource_test|graphics_benchmark]\n' "$0" >&2
}

setup_compiler() {
    if ! command -v "$CC" >/dev/null 2>&1 || ! command -v "$CXX" >/dev/null 2>&1; then
        printf 'C and C++ compilers are required (set CC and CXX).\n' >&2
        return 1
    fi
    if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists sdl3 vulkan; then
        printf 'SDL3 and Vulkan development packages are required (pkg-config: sdl3 vulkan).\n' >&2
        return 1
    fi
    read -r -a pkg_cflags <<< "$(pkg-config --cflags sdl3 vulkan)"
    read -r -a pkg_libs <<< "$(pkg-config --libs sdl3 vulkan)"
}

text_shaders() {
    local source
    for source in data/Sample.vert data/Sample.frag; do
        "$GLSLC_PATH" --target-env=vulkan1.3 "$source" -o "$source.spv"
    done
}

api_shaders() {
    local source
    for source in code/test/shaders/graphics_test.vert code/test/shaders/graphics_test.frag; do
        "$GLSLC_PATH" --target-env=vulkan1.3 "$source" -o "$source.spv"
    done
}

shaders() {
    text_shaders
    api_shaders
}

graphics_backend() {
    setup_compiler
    local source
    for source in "${graphics_sources[@]}"; do
        "$CC" -std=c11 -g "$graphics_opt" -Wall "${graphics_defines[@]}" -Icode "${pkg_cflags[@]}" -c "code/rendering/$source.c" -o "$source.o"
    done
}

vulkan_cube_test() {
    text_shaders
    graphics_defines=()
    graphics_backend
    "$CXX" -std=c++20 -g "$graphics_opt" -Wall -Icode "${pkg_cflags[@]}" code/test/vulkan_cube_test.cpp "${graphics_objects[@]}" "${pkg_libs[@]}" -lm -o vulkan_cube_test
    printf 'Font demo built. Run ./vulkan_cube_test --frames 120 or --interactive from the repository root.\n'
}

graphics_api_test() {
    api_shaders
    graphics_defines=(-DGRAPHICS_TESTING)
    graphics_backend
    "$CC" -std=c11 -g "$graphics_opt" -Wall "${graphics_defines[@]}" -Icode "${pkg_cflags[@]}" code/test/graphics_api_test.c "${graphics_objects[@]}" "${pkg_libs[@]}" -lm -o graphics_api_test
    printf 'Graphics API tests built. Run ./graphics_api_test from the repository root.\n'
}

graphics_resource_test() {
    graphics_defines=()
    graphics_backend
    "$CC" -std=c11 -g "$graphics_opt" -Wall -Icode "${pkg_cflags[@]}" code/test/graphics_resource_test.c "${graphics_objects[@]}" "${pkg_libs[@]}" -lm -o graphics_resource_test
}

graphics_benchmark() {
    graphics_opt=-O2
    graphics_defines=(-DGRAPHICS_BENCHMARK)
    graphics_backend
    "$CC" -std=c11 -g -O2 -Wall "${graphics_defines[@]}" -Icode "${pkg_cflags[@]}" code/test/graphics_benchmark.c "${graphics_objects[@]}" "${pkg_libs[@]}" -lm -o graphics_benchmark
}

all() {
    vulkan_cube_test
    graphics_resource_test
    graphics_api_test
    graphics_benchmark
}

clean() {
    local target extension
    for target in vulkan_cube_test graphics_api_test graphics_resource_test graphics_benchmark; do
        rm -f -- "./$target"
        for extension in exe pdb obj ilk; do
            rm -f -- "./$target.$extension"
        done
    done
    rm -f -- "${graphics_objects[@]}" "${graphics_objects[@]/%.o/.obj}" graphics_cube.o graphics_cube.obj \
        data/Sample.vert.spv data/Sample.frag.spv \
        code/test/shaders/graphics_test.vert.spv code/test/shaders/graphics_test.frag.spv
    printf 'Clean completed.\n'
}

if (( $# > 1 )); then
    usage
    exit 1
fi
case "${1:-all}" in
    all) all ;;
    debug) graphics_opt=-O0; all ;;
    release) graphics_opt=-O2; all ;;
    shaders) shaders ;;
    clean) clean ;;
    vulkan_cube_test) vulkan_cube_test ;;
    graphics_api_test) graphics_api_test ;;
    graphics_resource_test) graphics_resource_test ;;
    graphics_benchmark) graphics_benchmark ;;
    *) usage; exit 1 ;;
esac
