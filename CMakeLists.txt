cmake_minimum_required(VERSION 3.15)
project(base_project LANGUAGES C CXX)
include(CTest)
include(CMakeParseArguments)

option(USE_SDL "Enable SDL integration" ON)
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

include_directories(${CMAKE_SOURCE_DIR}/code)
add_compile_definitions(_POSIX_PTHREAD_SEMANTICS)

if(DEBUG_TRACE)
  message(STATUS "Debug trace enabled")
else()
  add_compile_definitions(NO_DEBUG_TRACE)
endif()

if(MSVC)
  set(TEST_COMPILE_OPTIONS /FC /Z7 /W3 /D_CRT_SECURE_NO_WARNINGS)
else()
  set(TEST_COMPILE_OPTIONS -g -Wall)
endif()

function(base_add_test target_name source_file)
  set(options NO_CTEST)
  set(oneValueArgs)
  set(multiValueArgs LINK_LIBS LABELS COMPILE_DEFINITIONS ARGS)
  cmake_parse_arguments(BASE_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  add_executable(${target_name} ${source_file})
  target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/code)
  target_compile_options(${target_name} PRIVATE ${TEST_COMPILE_OPTIONS})
  target_compile_definitions(${target_name} PRIVATE _POSIX_PTHREAD_SEMANTICS ${BASE_TEST_COMPILE_DEFINITIONS})
  target_link_libraries(${target_name} PRIVATE ${BASE_TEST_LINK_LIBS})

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
    if(NOT USE_SDL)
      message(FATAL_ERROR "Graphics tests require SDL3. Set ENABLE_GRAPHICS_TESTS=OFF for unit tests only.")
    endif()
    if(VULKAN_SDK)
      set(ENV{VULKAN_SDK} "${VULKAN_SDK}")
    endif()
    find_package(Vulkan REQUIRED)
    find_package(SDL3 CONFIG QUIET)
    if(NOT TARGET SDL3::SDL3)
      find_path(BASE_SDL_INCLUDE_DIR SDL3/SDL.h HINTS "${CMAKE_SOURCE_DIR}/code/third-party/SDL/include")
      find_library(BASE_SDL_LIBRARY NAMES SDL3 HINTS "$ENV{SDL_PATH}"
        "${CMAKE_SOURCE_DIR}/code/third-party/SDL/VisualC/x64/Debug")
      if(NOT BASE_SDL_INCLUDE_DIR OR NOT BASE_SDL_LIBRARY)
        message(FATAL_ERROR "SDL3 headers/library not found. Install SDL3 or build the bundled SDL project.")
      endif()
      add_library(SDL3::SDL3 UNKNOWN IMPORTED)
      set_target_properties(SDL3::SDL3 PROPERTIES IMPORTED_LOCATION "${BASE_SDL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BASE_SDL_INCLUDE_DIR}")
      if(WIN32)
        get_filename_component(BASE_SDL_LIBRARY_DIR "${BASE_SDL_LIBRARY}" DIRECTORY)
        find_file(BASE_SDL_DLL SDL3.dll HINTS "${BASE_SDL_LIBRARY_DIR}" NO_DEFAULT_PATH)
      endif()
    endif()
    set(GRAPHICS_TEST_LINK_LIBS Vulkan::Vulkan SDL3::SDL3)
    if(UNIX)
      list(APPEND GRAPHICS_TEST_LINK_LIBS m)
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
      code/rendering/graphics_upload.c code/rendering/graphics_lifetime.c code/rendering/graphics_command.c
      code/rendering/graphics_pipeline.c code/rendering/graphics_resources.c code/rendering/graphics_swapchain.c)
    target_sources(test_vulkan_cube PRIVATE ${GRAPHICS_BACKEND_SOURCES})
    base_add_test(graphics_resource_test code/test/graphics_resource_test.c
      NO_CTEST LABELS integration graphics LINK_LIBS ${GRAPHICS_TEST_LINK_LIBS})
    target_sources(graphics_resource_test PRIVATE ${GRAPHICS_BACKEND_SOURCES})
    base_add_test(graphics_api_test code/test/graphics_api_test.c
      NO_CTEST LABELS integration graphics LINK_LIBS ${GRAPHICS_TEST_LINK_LIBS} COMPILE_DEFINITIONS GRAPHICS_TESTING)
    target_sources(graphics_api_test PRIVATE ${GRAPHICS_BACKEND_SOURCES})
    base_add_test(graphics_benchmark code/test/graphics_benchmark.c
      NO_CTEST LINK_LIBS ${GRAPHICS_TEST_LINK_LIBS} COMPILE_DEFINITIONS GRAPHICS_BENCHMARK)
    target_sources(graphics_benchmark PRIVATE ${GRAPHICS_BACKEND_SOURCES})
    if(MSVC)
      target_compile_options(graphics_benchmark PRIVATE /O2)
    else()
      target_compile_options(graphics_benchmark PRIVATE -O2)
    endif()
    if(WIN32 AND BASE_SDL_DLL)
      foreach(GRAPHICS_TARGET test_vulkan_cube graphics_resource_test graphics_api_test graphics_benchmark)
        add_custom_command(TARGET ${GRAPHICS_TARGET} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BASE_SDL_DLL}" "$<TARGET_FILE_DIR:${GRAPHICS_TARGET}>"
          VERBATIM)
      endforeach()
    endif()
    if(REGISTER_GRAPHICS_TESTS)
      add_test(NAME graphics_resource_test COMMAND graphics_resource_test)
      set_tests_properties(graphics_resource_test PROPERTIES TIMEOUT 60 LABELS "integration;graphics")
      add_test(NAME graphics_api_test COMMAND graphics_api_test)
      set_tests_properties(graphics_api_test PROPERTIES TIMEOUT 60 LABELS "integration;graphics" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
    find_program(BASE_GLSLC NAMES glslc HINTS "${VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin")
    if(NOT BASE_GLSLC)
      message(FATAL_ERROR "glslc not found. Install the Vulkan SDK or set BASE_GLSLC.")
    endif()
    set(SAMPLE_SHADER_OUTPUTS)
    foreach(SAMPLE_SHADER Sample.vert Sample.frag)
      set(SAMPLE_SHADER_SOURCE "${CMAKE_SOURCE_DIR}/data/${SAMPLE_SHADER}")
      add_custom_command(
        OUTPUT "${SAMPLE_SHADER_SOURCE}.spv"
        COMMAND "${BASE_GLSLC}" --target-env=vulkan1.3 "${SAMPLE_SHADER_SOURCE}" -o "${SAMPLE_SHADER_SOURCE}.spv"
        DEPENDS "${SAMPLE_SHADER_SOURCE}"
        VERBATIM
      )
      list(APPEND SAMPLE_SHADER_OUTPUTS "${SAMPLE_SHADER_SOURCE}.spv")
    endforeach()
    add_custom_target(sample_text_shaders DEPENDS ${SAMPLE_SHADER_OUTPUTS})
    add_dependencies(test_vulkan_cube sample_text_shaders)
    set(GRAPHICS_API_SHADER_OUTPUTS)
    foreach(SHADER graphics_test.vert graphics_test.frag)
      set(SHADER_SOURCE "${CMAKE_SOURCE_DIR}/code/test/shaders/${SHADER}")
      add_custom_command(OUTPUT "${SHADER_SOURCE}.spv"
        COMMAND ${BASE_GLSLC} --target-env=vulkan1.3 "${SHADER_SOURCE}" -o "${SHADER_SOURCE}.spv"
        DEPENDS "${SHADER_SOURCE}" VERBATIM)
      list(APPEND GRAPHICS_API_SHADER_OUTPUTS "${SHADER_SOURCE}.spv")
    endforeach()
    add_custom_target(graphics_api_shaders DEPENDS ${GRAPHICS_API_SHADER_OUTPUTS})
    add_dependencies(graphics_api_test graphics_api_shaders)
    if(REGISTER_GRAPHICS_TESTS)
      set_tests_properties(test_vulkan_cube PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
      set_property(TEST test_vulkan_cube PROPERTY TIMEOUT 60)
    endif()
  endif()

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_allocator test_vector test_ui_core
    USES_TERMINAL
  )
  if(ENABLE_GRAPHICS_TESTS AND REGISTER_GRAPHICS_TESTS)
    add_dependencies(check test_vulkan_cube graphics_resource_test graphics_api_test)
  endif()
endif()


message(STATUS "CMake configuration finished.")
message(STATUS "Build tests: cmake --build build --target test_allocator test_vector")
message(STATUS "Run tests: ctest --test-dir build --output-on-failure")
message(STATUS "Build graphics test: cmake --build build --target test_vulkan_cube")
