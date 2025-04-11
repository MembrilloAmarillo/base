#!/bin/bash

set -xe

mkdir -p build
pushd build

if [ "$1" == "directWriteD2D1Sample" ]; then
    cl /Zii ../samples/directWriteD2D1/samples.cpp /Fe:directWriteD2D1Sample.exe
elif [ "$1" == "memorySample" ]; then
    clang++ -ggdb -Wall -DDEBUG -std=c++20 ../samples/memory/memory_test.cpp -o memory_test
elif [ "$1" == "vectorSample" ]; then
    clang++ -ggdb -Wall -DDEBUG -std=c++20 ../samples/sample_vector/sample_vector.cpp -o sample_vector
elif [ "$1" == "vulkanSample" ]; then
    glslc ../samples/vulkan_sample/compute.comp -o ../samples/vulkan_sample/compute.comp.spv
    glslc ../samples/vulkan_sample/ColoredTriangle.vert -o ../samples/vulkan_sample/ColoredTriangle.vert.spv
    glslc ../samples/vulkan_sample/ColoredTriangle.frag -o ../samples/vulkan_sample/ColoredTriangle.frag.spv

    clang++ -ggdb -Wall -DDEBUG -std=c++20 ../samples/vulkan_sample/vulkan_sample.cpp \
        -I /home/polaris/devel/VulkanSDK/Include/ \
        -o vulkan_sample
else
    echo "Invalid argument. Please provide one of the following:"
    echo "  directWriteD2D1Sample"
    echo "  memorySample"
    echo "  vectorSample"
    echo "  vulkanSample"
fi

popd
