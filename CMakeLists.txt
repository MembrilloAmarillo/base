cmake_minimum_required(VERSION 3.15)
project(base_project LANGUAGES C CXX)

option(USE_SDL "Enable SDL integration" OFF)
option(USE_FREETYPE "Enable FreeType" OFF)
option(DEBUG_TRACE "Enable tracing for output debugging" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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


find_library(Slang_LIBRARY NAMES slang slang-compiler HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)

# SDL integration (optional)
if(USE_SDL)
  find_library(SDL_LIBRARY NAMES SDL3 HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
  add_compile_definitions(SDL_USAGE)
endif()
if(SDL_LIBRARY)
  message(STATUS "SDL Found")
else()
  message(STATUS "SDL Not Found")
endif()
if(USE_FREETYPE)
  find_library(FREETYPE_LIBRARY freetype)
  include_directories(/usr/include/freetype2)
  add_compile_definitions(USE_FREETYPE)
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
else()
  list(APPEND COMMON_LIBS slang)
endif()
if(USE_SDL AND SDL3_LIBS_LIST)
  # SDL3 libs string may contain flags; attach as INTERFACE_LINK_LIBRARIES when creating targets
  set(SDL3_RAW_LIBS ${SDL3_LIBS_LIST})
endif()

if(DEBUG_TRACE)
  message(STATUS "Debug trace enabled")
else()
  add_compile_definitions(NO_DEBUG_TRACE)
endif()

# test_allocator
add_executable(test_allocator code/test/allocator_test.cpp)
target_include_directories(test_allocator PRIVATE ${CMAKE_SOURCE_DIR}/code)
target_link_libraries(test_allocator PRIVATE ${COMMON_LIBS})
target_compile_options(test_allocator PRIVATE -g -ggdb -DDEBUG -Wall -fdiagnostics-all-candidates)

# test_vector
add_executable(test_vector code/test/vector_test.cpp)
target_include_directories(test_vector PRIVATE ${CMAKE_SOURCE_DIR}/code)
target_link_libraries(test_vector PRIVATE ${COMMON_LIBS})
target_compile_options(test_vector PRIVATE -g -ggdb -DDEBUG -Wall -fdiagnostics-all-candidates)

# test_vulkan: test_vulkan depends on ktxlib
add_executable(test_vulkan code/test/vulkan_test.cpp)
target_include_directories(test_vulkan PRIVATE ${CMAKE_SOURCE_DIR}/code)
if(KTX_C_SOURCES)
  target_link_libraries(test_vulkan PRIVATE ktxlib)
endif()

if(USE_SDL)
  # SDL3 raw libs string may contain multiple tokens; add them
  separate_arguments(SDL3_RAW_LIBS)
  target_link_libraries(test_vulkan PRIVATE ${SDL_LIBRARY})
endif()

target_compile_options(test_vulkan PRIVATE -g -ggdb -DDEBUG -Wall -fdiagnostics-all-candidates)
# Define VOLK_IMPLEMENTATION only in the test translation unit, not globally, to avoid
# duplicate definition warnings. The test sources already define it; do not force it here.
target_compile_definitions(test_vulkan PRIVATE _POSIX_PTHREAD_SEMANTICS)
target_link_libraries(test_vulkan PRIVATE ${COMMON_LIBS})

# test_vulkan_perf: performance profiling test
add_executable(test_vulkan_perf code/test/vulkan_perf.cpp)
target_include_directories(test_vulkan_perf PRIVATE ${CMAKE_SOURCE_DIR}/code)
if(KTX_C_SOURCES)
  target_link_libraries(test_vulkan_perf PRIVATE ktxlib)
endif()

if(USE_SDL)
  separate_arguments(SDL3_RAW_LIBS)
  target_link_libraries(test_vulkan_perf PRIVATE ${SDL_LIBRARY})
endif()

target_compile_options(test_vulkan_perf PRIVATE -g -ggdb -DDEBUG -Wall -fdiagnostics-all-candidates -O2)
target_compile_definitions(test_vulkan_perf PRIVATE _POSIX_PTHREAD_SEMANTICS)
target_link_libraries(test_vulkan_perf PRIVATE ${COMMON_LIBS})


message(STATUS "CMake configuration finished. Use: mkdir build && cmake -S . -B build -DUSE_SDL=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target test_vulkan")
