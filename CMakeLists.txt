cmake_minimum_required(VERSION 3.16)
project(BaseProject LANGUAGES CXX)

add_executable(memorySample samples/memory/memory_test.cpp)
target_include_directories(memorySample
    PRIVATE
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_SOURCE_DIR}/memory/
)

add_executable(vectorSample samples/sample_vector/sample_vector.cpp)
target_include_directories(vectorSample
    PRIVATE
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_SOURCE_DIR}/memory/
        ${CMAKE_SOURCE_DIR}/vector/
)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Glfw include dirs: ${glfw_INCLUDE_DIRS}")
add_executable(vulkanSample samples/vulkan_sample/vulkan_sample.cpp)
target_include_directories(vulkanSample
    PRIVATE
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_SOURCE_DIR}/render/
        ${CMAKE_SOURCE_DIR}/memory/
        ${CMAKE_SOURCE_DIR}/vector/
        ${Vulkan_INCLUDE_DIRS}
        ${glfw3_INCLUDE_DIRS}
)
target_link_libraries(vulkanSample PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(vulkanSample PRIVATE glfw)


# List your shader files
set(SAMPLE_SHADER_SRC
    ${CMAKE_SOURCE_DIR}/samples/vulkan_sample/ColoredTriangle.vert
    ${CMAKE_SOURCE_DIR}/samples/vulkan_sample/ColoredTriangle.frag
    ${CMAKE_SOURCE_DIR}/samples/vulkan_sample/compute.comp
)

set(SAMPLE_SHADER_BIN)
foreach(SHADER ${SAMPLE_SHADER_BIN})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV ${CMAKE_SOURCE_DIR}/samples/vulkan_sample/${FILE_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${FILE_NAME}"
        VERBATIM
    )
    list(APPEND SAMPLE_SHADER_BIN ${SPIRV})
endforeach()

# Custom target to build all shaders
add_custom_target(build_sample_shaders ALL DEPENDS ${SAMPLE_SHADER_BIN})

# Make your vulkanSample target depend on the shaders
add_dependencies(vulkanSample build_sample_shaders)
