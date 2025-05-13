cmake_minimum_required(VERSION 3.16)
project(BaseProject LANGUAGES CXX)


add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/pugixml)

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

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

option(TRACY_ENABLE " " ON)
option(TRACY_ON_DEMAND " " ON)
add_subdirectory (third_party/tracy)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
set(Vulkan_INCLUDE_DIR ${VULKAN_SDK}/include)
message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")
add_executable(vulkanSample samples/vulkan_sample/vulkan_sample.cpp)
target_include_directories(vulkanSample
    PRIVATE
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_SOURCE_DIR}/render/
        ${CMAKE_SOURCE_DIR}/memory/
        ${CMAKE_SOURCE_DIR}/vector/
	${CMAKE_SOURCE_DIR}/third_party/
	${IMGUI_DIR}
    	${IMGUI_DIR}/backends
        ${Vulkan_INCLUDE_DIRS}
        ${glfw3_INCLUDE_DIRS}
)
target_link_libraries(vulkanSample PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(vulkanSample PRIVATE glfw)
target_link_libraries(vulkanSample PRIVATE imgui)
target_link_libraries(vulkanSample PRIVATE Tracy::TracyClient)

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
