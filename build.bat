@echo off

set cl_libs=/LIBPATH:C:\devel\glfw\lib-vc2022\ /LIBPATH:C:\devel\VulkanSDK\Lib\

mkdir build
pushd build

IF "%1" == "directWriteD2D1Sample" (
	cl /Zii ..\samples\directWriteD2D1\samples.cpp /Fe:directWriteD2D1Sample.exe
) ELSE IF "%1" == "memorySample" (
	cl /Zii ..\samples\memory\memory_test.cpp /Fe:memory_test.exe
) ELSE IF "%1" == "vectorSample" (
	cl /Zii ..\samples\sample_vector\sample_vector.cpp /Fe:sample_vector.exe
) ELSE IF "%1" == "vulkanSample" (
	cl /Zii /MD /DDEBUG /std:c++20 ..\samples\vulkan_sample\vulkan_sample.cpp /I C:\devel\glfw\include\ /I C:\devel\VulkanSDK\Include\ /link %cl_libs% /Fe:vulkan_sample.exe
)

popd