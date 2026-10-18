cmake_minimum_required(VERSION 3.15)
project(base_project LANGUAGES C CXX)
include(CTest)
include(CMakeParseArguments)

option(USE_SDL "Enable SDL integration" ON)
option(USE_FREETYPE "Enable FreeType" ON)
option(DEBUG_TRACE "Enable tracing for output debugging" OFF)
option(USE_CPP23_WHEN_AVAILABLE "Try to compile in C++23 mode when compiler+cmake support it" ON)
option(USE_CPP26_WHEN_AVAILABLE "Try to compile in C++26 mode when compiler+cmake support it" OFF)
option(BASE_ENABLE_TESTS "Build and register project tests" ON)
option(ENABLE_GRAPHICS_TESTS "Build graphics/window integration tests" ON)
option(REGISTER_GRAPHICS_TESTS "Register graphics/window integration tests in CTest" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(PROJECT_CXX_STANDARD 20)
if(USE_CPP26_WHEN_AVAILABLE)
  if("cxx_std_26" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(PROJECT_CXX_STANDARD 26)
  elseif(USE_CPP23_WHEN_AVAILABLE AND "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(PROJECT_CXX_STANDARD 23)
  endif()
elseif(USE_CPP23_WHEN_AVAILABLE)
  if("cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(PROJECT_CXX_STANDARD 23)
  endif()
endif()

set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "Configured C++ standard: C++${CMAKE_CXX_STANDARD}")

# Allow user to pass VULKAN_SDK path. If not set, try environment.
if(NOT VULKAN_SDK AND DEFINED ENV{VULKAN_SDK})
  set(VULKAN_SDK $ENV{VULKAN_SDK})
endif()

if (NOT IS_DIRECTORY $ENV{VULKAN_SDK})
    message(WARNING "Vulkan SDK not detected, if installed please set VULKAN_SDK env var or manually provide libraries")
else()
    message(STATUS "Found Vulkan SDK in $ENV{VULKAN_SDK}")
endif()

if(VULKAN_SDK)
  include_directories(${VULKAN_SDK}/include)
  link_directories(${VULKAN_SDK}/lib)
endif()

# Common include dirs and compile defs
include_directories(
  ${CMAKE_SOURCE_DIR}/code
  ${CMAKE_SOURCE_DIR}/code/third-party/ktx/include
  ${CMAKE_SOURCE_DIR}/code/third-party/ktx/other_include
)
add_compile_definitions(_POSIX_PTHREAD_SEMANTICS)

# Find Vulkan
find_package(Vulkan QUIET)
if(TARGET Vulkan::Vulkan)
  message(STATUS "Found Vulkan target Vulkan::Vulkan")
  set(VULKAN_LIBS Vulkan::Vulkan)
elseif(Vulkan_FOUND)
  # older CMake modules may set Vulkan_FOUND and Vulkan_LIBRARY
  if(DEFINED Vulkan_LIBRARY)
    message(STATUS "Found Vulkan library: ${Vulkan_LIBRARY}")
    set(VULKAN_LIBS ${Vulkan_LIBRARY})
  else()
    message(STATUS "Vulkan found but no target; falling back to -lvulkan")
    set(VULKAN_LIBS vulkan)
  endif()
else()
  message(STATUS "Vulkan not found by CMake; linking -lvulkan and relying on VULKAN_SDK if set")
  find_library(VULKAN_LIB vulkan)
  if(VULKAN_LIB)
    set(VULKAN_LIBS ${VULKAN_LIB})
  else()
    set(VULKAN_LIBS vulkan)
  endif()
endif()

find_library(SPIRV_LIBRARY NAMES spirv-reflect-static spirvreflect  HINTS "$ENV{VULKAN_SDK}/lib")
find_library(SLANG_LIBRARY NAMES slang slang-compiler HINTS "$ENV{VULKAN_SDK}/lib")
find_package(X11 QUIET)

if(SLANG_LIBRARY)
  message(STATUS "Found Slang library: ${SLANG_LIBRARY}")
else()
  message(STATUS "Slang library not found; targets that do not use runtime shader compilation will not link it")
endif()

if(SPIRV_LIBRARY)
  message(STATUS "Found SPIR-V reflection library: ${SPIRV_LIBRARY}")
else()
  message(STATUS "SPIR-V reflection library not found; skipping optional link library")
endif()

if(X11_FOUND)
  message(STATUS "Found X11")
else()
  message(STATUS "X11 not found by CMake; Linux window tests may require manual X11 library configuration")
endif()

# SDL integration (optional)
if(USE_SDL)
  find_package(SDL3 CONFIG QUIET)
  if(TARGET SDL3::SDL3)
    set(SDL_LIBRARY SDL3::SDL3)
  else()
    find_library(SDL_LIBRARY NAMES SDL3 HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
  endif()
  add_compile_definitions(SDL_USAGE)
endif()
if(SDL_LIBRARY)
  message(STATUS "SDL Found")
else()
  message(STATUS "SDL Not Found")
endif()
if(USE_FREETYPE)
  find_package(Freetype QUIET)
  if(TARGET Freetype::Freetype)
    set(FREETYPE_LIBRARY Freetype::Freetype)
  else()
    find_library(FREETYPE_LIBRARY freetype REQUIRED)
    include_directories(/usr/include/freetype2)
  endif()
  add_compile_definitions(USE_FREETYPE)
endif()
if(FREETYPE_LIBRARY)
  message(STATUS "FreeType Found")
else()
  message(STATUS "FreeType Not Found")
endif()

# KTX: build all C sources under code/third-party/ktx/lib as a static lib
file(GLOB KTX_C_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "code/third-party/ktx/lib/*.c")
if(KTX_C_SOURCES)
  add_library(ktxlib STATIC ${KTX_C_SOURCES})
  target_include_directories(ktxlib PUBLIC ${CMAKE_SOURCE_DIR}/code/third-party/ktx/include ${CMAKE_SOURCE_DIR}/code/third-party/ktx/other_include)
  target_compile_options(ktxlib PRIVATE)
endif()

# Common link libraries
set(COMMON_LIBS ${VULKAN_LIBS} m pthread dl rt)
if(SLANG_LIBRARY)
  list(APPEND COMMON_LIBS ${SLANG_LIBRARY})
endif()

if(SPIRV_LIBRARY)
  list(APPEND COMMON_LIBS ${SPIRV_LIBRARY})
endif()


if(DEBUG_TRACE)
  message(STATUS "Debug trace enabled")
else()
  add_compile_definitions(NO_DEBUG_TRACE)
endif()

set(TEST_COMPILE_OPTIONS
  -g
  -ggdb
  -DDEBUG
  -Wall
  -fdiagnostics-all-candidates
)

function(base_add_test target_name source_file)
  set(options NO_CTEST)
  set(oneValueArgs)
  set(multiValueArgs LINK_LIBS LABELS COMPILE_DEFINITIONS ARGS)
  cmake_parse_arguments(BASE_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  add_executable(${target_name} ${source_file})
  target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/code)
  target_compile_options(${target_name} PRIVATE ${TEST_COMPILE_OPTIONS})
  target_compile_definitions(${target_name} PRIVATE _POSIX_PTHREAD_SEMANTICS ${BASE_TEST_COMPILE_DEFINITIONS})
  target_link_libraries(${target_name} PRIVATE ${COMMON_LIBS} ${BASE_TEST_LINK_LIBS})

  if(NOT BASE_TEST_NO_CTEST)
    add_test(NAME ${target_name} COMMAND ${target_name} ${BASE_TEST_ARGS})
    if(BASE_TEST_LABELS)
      set_tests_properties(${target_name} PROPERTIES LABELS "${BASE_TEST_LABELS}")
    endif()
  endif()
endfunction()

if(BASE_ENABLE_TESTS)
  enable_testing()
  base_add_test(test_allocator code/test/allocator_test.cpp LABELS unit)
  base_add_test(test_vector code/test/vector_test.cpp LABELS unit)
  base_add_test(test_ui_core code/test/ui_core_test.cpp LABELS unit)

  if(ENABLE_GRAPHICS_TESTS)
    set(GRAPHICS_TEST_LINK_LIBS)
    if(USE_SDL)
      list(APPEND GRAPHICS_TEST_LINK_LIBS ${SDL_LIBRARY})
    endif()
    if(USE_FREETYPE)
      list(APPEND GRAPHICS_TEST_LINK_LIBS ${FREETYPE_LIBRARY})
    endif()
    if(TARGET X11::X11)
      list(APPEND GRAPHICS_TEST_LINK_LIBS X11::X11)
    elseif(X11_FOUND)
      list(APPEND GRAPHICS_TEST_LINK_LIBS ${X11_LIBRARIES})
    endif()
    if(TARGET ktxlib)
      list(APPEND GRAPHICS_TEST_LINK_LIBS ktxlib)
    endif()

    if(REGISTER_GRAPHICS_TESTS)
      base_add_test(test_vulkan_cube code/test/vulkan_cube_test.cpp
        LABELS integration graphics
        LINK_LIBS ${GRAPHICS_TEST_LINK_LIBS}
        ARGS --frames 120
      )
    else()
      base_add_test(test_vulkan_cube code/test/vulkan_cube_test.cpp
        NO_CTEST
        LABELS integration graphics
        LINK_LIBS ${GRAPHICS_TEST_LINK_LIBS}
      )
    endif()
    set(GRAPHICS_BACKEND_SOURCES code/rendering/graphics.c code/rendering/graphics_memory.c
      code/rendering/graphics_upload.c code/rendering/graphics_lifetime.c)
    target_sources(test_vulkan_cube PRIVATE ${GRAPHICS_BACKEND_SOURCES})
    base_add_test(graphics_resource_test code/test/graphics_resource_test.c
      NO_CTEST LABELS integration graphics LINK_LIBS ${GRAPHICS_TEST_LINK_LIBS})
    target_sources(graphics_resource_test PRIVATE ${GRAPHICS_BACKEND_SOURCES})
    if(REGISTER_GRAPHICS_TESTS)
      add_test(NAME graphics_resource_test COMMAND graphics_resource_test)
      set_tests_properties(graphics_resource_test PROPERTIES TIMEOUT 60 LABELS "integration;graphics")
    endif()
    find_program(BASE_GLSLC NAMES glslc HINTS "${VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" REQUIRED)
    set(SAMPLE_SHADER_OUTPUTS)
    foreach(SAMPLE_SHADER Sample.vert Sample.frag)
      set(SAMPLE_SHADER_SOURCE "${CMAKE_SOURCE_DIR}/data/${SAMPLE_SHADER}")
      add_custom_command(
        OUTPUT "${SAMPLE_SHADER_SOURCE}.spv"
        COMMAND "${BASE_GLSLC}" --target-env=vulkan1.2 "${SAMPLE_SHADER_SOURCE}" -o "${SAMPLE_SHADER_SOURCE}.spv"
        DEPENDS "${SAMPLE_SHADER_SOURCE}"
        VERBATIM
      )
      list(APPEND SAMPLE_SHADER_OUTPUTS "${SAMPLE_SHADER_SOURCE}.spv")
    endforeach()
    add_custom_target(sample_text_shaders DEPENDS ${SAMPLE_SHADER_OUTPUTS})
    add_dependencies(test_vulkan_cube sample_text_shaders)
    if(REGISTER_GRAPHICS_TESTS)
      set_tests_properties(test_vulkan_cube PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
      set_property(TEST test_vulkan_cube PROPERTY TIMEOUT 60)
    endif()
  endif()

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_allocator test_vector
    USES_TERMINAL
  )
endif()


message(STATUS "CMake configuration finished.")
message(STATUS "Build tests: cmake --build build --target test_allocator test_vector")
message(STATUS "Run tests: ctest --test-dir build --output-on-failure")
message(STATUS "Build graphics test: cmake --build build --target test_vulkan_cube")
