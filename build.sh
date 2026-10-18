#!/usr/bin/env bash
set -euo pipefail

# Run from the repository root, regardless of the caller's working directory.
# SDL3 and Vulkan headers/libraries are discovered via pkg-config.
# CC, CXX and GLSLC_PATH can be overridden in the environment.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

CC=${CC:-cc}
CXX=${CXX:-c++}
GLSLC_PATH=${GLSLC_PATH:-glslc}

c_debug=(-std=c11 -g -O0 -DDEBUG -Wall)
cxx_debug=(-std=c++17 -g -O0 -DDEBUG -Wall)
c_release=(-std=c11 -O2 -Wall)
cxx_release=(-std=c++17 -O2 -Wall)
includes=(-Icode)
pkg_cflags=()
pkg_libs=()
libs=(-lm -pthread -ldl)
graphics_objects=(graphics_cube.o graphics_memory.o graphics_upload.o graphics_lifetime.o)

usage() {
    printf 'Usage: %s [all|debug|release|shaders|clean|RenderLibrary|DrawSample|todolist|csv_test|obj_test|vulkan_cube_test|graphics_api_test|graphics_resource_test] [SDL_USAGE]\n' "$0" >&2
}

require_sources() {
    local source
    for source in "$@"; do
        if [[ ! -f $source ]]; then
            printf 'Missing source: %s\n' "$source" >&2
            return 1
        fi
    done
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

compile_vma() {
    local flags=$1 output=$2
    if [[ $flags == release ]]; then
        "$CXX" "${cxx_release[@]}" "${includes[@]}" "${pkg_cflags[@]}" -x c++ -c code/third-party/vk_mem_alloc.c -o "$output"
    else
        "$CXX" "${cxx_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" -x c++ -c code/third-party/vk_mem_alloc.c -o "$output"
    fi
}

compile_xxhash() {
    "$CC" "${c_debug[@]}" "${includes[@]}" -c code/third-party/xxhash.c -o xxhash.o
}

compile_cpp_objects() {
    "$CXX" "${cxx_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" -x c++ -c code/third-party/xxhash.c -o xxhash.o
    compile_vma debug vk_mem_alloc.o
}

debug() {
    require_sources code/Samples/UI_Sample.c
    setup_compiler
    compile_vma debug vk_mem_alloc.o
    compile_xxhash
    "$CC" "${c_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" -c code/Samples/UI_Sample.c -o UI_Sample.o
    "$CXX" UI_Sample.o xxhash.o vk_mem_alloc.o "${pkg_libs[@]}" "${libs[@]}" -o UI_Sample
    printf 'Debug build completed.\n'
}

render() {
    require_sources code/Samples/RenderLibrary.c
    setup_compiler
    compile_vma debug vk_mem_alloc.o
    compile_xxhash
    "$CC" "${c_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" -c code/Samples/RenderLibrary.c -o RenderLibrary.o
    "$CXX" RenderLibrary.o xxhash.o vk_mem_alloc.o "${pkg_libs[@]}" "${libs[@]}" -o RenderLibrary
    printf 'RenderLibrary build completed.\n'
}

draw_sample() {
    require_sources code/Samples/DrawSample.c
    setup_compiler
    compile_vma debug vk_mem_alloc.o
    compile_xxhash
    "$CC" "${c_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" -c code/Samples/DrawSample.c -o DrawSample.o
    "$CXX" DrawSample.o xxhash.o vk_mem_alloc.o "${pkg_libs[@]}" "${libs[@]}" -o DrawSample
    printf 'DrawSample build completed.\n'
}

cpp_sample() {
    local source=$1 output=$2
    require_sources "$source"
    setup_compiler
    compile_cpp_objects
    local definitions=()
    if [[ $SDL_USAGE == SDL_USAGE ]]; then
        definitions=(-DSDL_USAGE)
    fi
    "$CXX" "${cxx_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" "${definitions[@]}" "$source" xxhash.o vk_mem_alloc.o "${pkg_libs[@]}" "${libs[@]}" -o "$output"
    printf '%s build completed.\n' "$output"
}

csv_test() {
    require_sources code/Samples/CsvTest.cpp
    setup_compiler
    "$CXX" "${cxx_release[@]}" "${includes[@]}" "${pkg_cflags[@]}" code/Samples/CsvTest.cpp "${pkg_libs[@]}" "${libs[@]}" -o csv_test
    printf 'csv_test build completed.\n'
}

release() {
    require_sources code/Samples/UI_Sample.c
    setup_compiler
    compile_vma release vk_mem_alloc_release.o
    "$CC" "${c_release[@]}" "${includes[@]}" "${pkg_cflags[@]}" -c code/Samples/UI_Sample.c -o UI_Sample_release.o
    "$CXX" UI_Sample_release.o vk_mem_alloc_release.o "${pkg_libs[@]}" "${libs[@]}" -o UI_Sample_Release
    printf 'Release build completed.\n'
}

shader() {
    local source=$1
    "$GLSLC_PATH" "$source" -o "$source.spv"
}

shaders() {
    if ! command -v "$GLSLC_PATH" >/dev/null 2>&1; then
        printf 'Warning: glslc not found (%s); skipping shaders.\n' "$GLSLC_PATH" >&2
        return 0
    fi
    local source
    for source in \
        data/compute.comp data/ColoredTriangle.vert data/ColoredTriangle.frag \
        data/ui_render.vert data/ui_render.frag data/Sample.vert data/Sample.frag \
        data/line.vert data/line.frag; do
        require_sources "$source"
        shader "$source"
    done
    for source in code/Samples/shaders/obj_showcase.vert code/Samples/shaders/obj_showcase.frag; do
        if [[ -f $source ]]; then
            shader "$source"
        fi
    done
    printf 'Shader compilation completed.\n'
}

graphics_test_backend() {
    if ! command -v "$GLSLC_PATH" >/dev/null 2>&1; then
        printf 'glslc not found: %s\n' "$GLSLC_PATH" >&2
        return 1
    fi
    local source
    for source in cube.vert cube.frag cube_ui.vert cube_ui.frag; do
        local path="code/test/shaders/$source"
        if [[ ! -f $path.spv || $path -nt $path.spv ]]; then
            shader "$path"
        fi
    done
    "$CC" "${c_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" -c code/rendering/graphics.c -o graphics_cube.o
    for source in graphics_memory graphics_upload graphics_lifetime; do
        "$CC" "${c_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" -c "code/rendering/$source.c" -o "$source.o"
    done
}

vulkan_cube_test() {
    setup_compiler
    graphics_test_backend
    local source
    for source in data/Sample.vert data/Sample.frag; do
        "$GLSLC_PATH" --target-env=vulkan1.2 "$source" -o "$source.spv"
    done
    "$CXX" -std=c++20 -g -ggdb -O0 -Wall "${includes[@]}" "${pkg_cflags[@]}" code/test/vulkan_cube_test.cpp "${graphics_objects[@]}" "${pkg_libs[@]}" "${libs[@]}" -o vulkan_cube_test
    printf 'Vulkan cube test built. Run ./vulkan_cube_test --frames 120 or --interactive.\n'
}

graphics_api_test() {
    setup_compiler
    graphics_test_backend
    "$CC" "${c_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" code/test/graphics_api_test.c "${graphics_objects[@]}" "${pkg_libs[@]}" "${libs[@]}" -o graphics_api_test
    printf 'Graphics API tests built. Run ./graphics_api_test from the repository root.\n'
}

graphics_resource_test() {
    setup_compiler
    graphics_test_backend
    "$CC" "${c_debug[@]}" "${includes[@]}" "${pkg_cflags[@]}" code/test/graphics_resource_test.c "${graphics_objects[@]}" "${pkg_libs[@]}" "${libs[@]}" -o graphics_resource_test
}

clean() {
    rm -f -- ./*.o ./*.obj ./*.exe ./*.pdb ./*.ilk \
        ./UI_Sample ./UI_Sample_Release ./RenderLibrary ./DrawSample \
        ./todolist ./csv_test ./ObjLoad ./vulkan_cube_test ./graphics_api_test ./graphics_resource_test \
        ./data/*.spv ./code/Samples/shaders/*.spv
    local artifact
    for artifact in ./code/test/shaders/*.spv; do
        if [[ -f $artifact ]] && ! git ls-files --error-unmatch -- "$artifact" >/dev/null 2>&1; then
            rm -f -- "$artifact"
        fi
    done
    printf 'Clean completed.\n'
}

all() {
    if [[ -d code/Samples ]]; then
        debug
        render
        draw_sample
    else
        printf 'Skipping sample executables: code/Samples is absent.\n' >&2
    fi
    shaders
}

target=${1:-all}
if (( $# > 2 )) || { (( $# == 2 )) && [[ $2 != SDL_USAGE ]]; }; then
    usage
    exit 1
fi
SDL_USAGE=${2:-}

case "$target" in
    all) all ;;
    debug) debug ;;
    release) release ;;
    shaders) shaders ;;
    clean) clean ;;
    RenderLibrary) render ;;
    DrawSample) draw_sample ;;
    todolist) cpp_sample code/Samples/ToDoList.cpp todolist ;;
    csv_test) csv_test ;;
    obj_test) cpp_sample code/Samples/ObjLoad.cpp ObjLoad ;;
    vulkan_cube_test) vulkan_cube_test ;;
    graphics_api_test) graphics_api_test ;;
    graphics_resource_test) graphics_resource_test ;;
    *) usage; exit 1 ;;
esac
