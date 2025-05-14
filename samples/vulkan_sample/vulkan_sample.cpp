#ifdef _WIN32
#pragma comment( lib, "vulkan-1" )
#pragma comment( lib, "user32" )
#pragma comment( lib, "gdi32" )
#pragma comment( lib, "glfw3" )
#pragma comment( lib, "kernel32" )
#pragma comment( lib, "shell32" )
#include <Windows.h>
#endif

#define IMGUI_DEFINE_MATH_OPERATORS

#include "render/vulkan_impl.cpp"

#include "imgui/imgui.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_widgets.cpp"
#include "imgui/imgui_tables.cpp"
#include "imgui/backends/imgui_impl_vulkan.cpp"
#include "imgui/backends/imgui_impl_glfw.cpp"

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

	vk_pipeline_builder builder(vIface.RenderArena, 2);
	builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//builder.SetPolygonMode(VK_POLYGON_MODE_LINE);
	builder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	builder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	builder.DisableBlending();
	builder.DisableDepthTest();
	builder.SetMultisamplingNone();

	vIface.AddPipeline(
		builder,
		"./samples/vulkan_sample/ColoredTriangle.vert.comp",
		"./samples/vulkan_sample/ColoredTriangle.frag.comp"
	);

	while (!vIface.Window.Close) {
		glfwPollEvents(); // Process window events (key press, mouse move, close button, etc.)
		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		//some imgui UI to test
		//ImGui::ShowDemoWindow();

		ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        // 2) Grab the ImGui IO struct and read Framerate
        ImGuiIO& io = ImGui::GetIO();
        float fps         = io.Framerate;               // frames per second
        float ms_per_frame = 1000.0f / (fps > 0.0f ? fps : 1.0f);

        // 3) Display them
        ImGui::Text("FPS:          %.1f", fps);
        ImGui::Text("Frame Time:  %.3f ms", ms_per_frame);

        // 4) End the window
        ImGui::End();

		//make imgui calculate internal draw structures
		ImGui::Render();

		// Render your Vulkan frame here
		vIface.BeginDrawing();
		//glfwSwapBuffers(vIface.Window.Window);
	}

	return 0;
}
