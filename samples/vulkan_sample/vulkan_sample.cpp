#ifdef _WIN32
#pragma comment( lib, "vulkan-1" )
#pragma comment( lib, "user32" )
#pragma comment( lib, "gdi32" )
#pragma comment( lib, "glfw3" )
#pragma comment( lib, "kernel32" )
#pragma comment( lib, "shell32" )
#include <Windows.h>
#endif

#include "../../render/vulkan_impl.cpp"

#include "../../math/math.cpp"

#ifdef __linux__
#include "../../memory/linux/memory_linux_impl.cpp"
#elif _WIN32
#include "../../memory/win/memory_win_impl.cpp"
#endif

#include "../../memory/memory.cpp"

#include "../../vector/vector.cpp"

int main() {

	vulkan_iface vIface("Hello World");

	while (!glfwWindowShouldClose(vIface.Window.Window)) {
		glfwPollEvents(); // Process window events (key press, mouse move, close button, etc.)

		// Render your Vulkan frame here
		vIface.BeginDrawing();
	}
	return 0;
}