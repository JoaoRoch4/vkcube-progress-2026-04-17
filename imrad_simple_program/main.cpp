#include <cstdlib>
#include <print>

#include <SDL3/SDL.h>

#include "app/platform/sdl_window.hpp"
#include "app/renderer/vulkan/vulkan_context.hpp"
#include "app/ui/imgui_layer.hpp"
#include "simple_window.h"

namespace {

class SimpleProgram {
    public:
	SimpleProgram();
	int Run();

    private:
	bool Init();
	void Shutdown();
	void HandleEvent(const SDL_Event* event);
	void Frame();

	SDLWindow m_sdl;
	VulkanContext m_vulkan;
	ImGuiLayer m_imgui;
	bool m_done;
	int m_w;
	int m_h;
};

SimpleProgram::SimpleProgram()
    : m_done { false }
    , m_w { 0 }
    , m_h { 0 }
{
}

bool SimpleProgram::Init()
{
	if (!m_sdl.Init("ImRAD Simple Program", 960, 640)) {
		std::println(stderr, "SDLWindow::Init failed");
		return false;
	}

	m_vulkan.Setup(m_sdl.GetVulkanExtensions());
	const VkSurfaceKHR surface = m_sdl.CreateVulkanSurface(m_vulkan.Instance, m_vulkan.Allocator);
	if (surface == VK_NULL_HANDLE) {
		std::println(stderr, "CreateVulkanSurface failed");
		return false;
	}

	m_sdl.GetSize(m_w, m_h);
	m_vulkan.SetupWindow(surface, m_w, m_h);
	m_sdl.Show();

	ImGui_ImplVulkan_InitInfo init_info = m_vulkan.MakeInitInfo();
	m_imgui.Init(m_sdl.Window, init_info, m_sdl.MainScale);
	simpleWindow.Open();
	return true;
}

void SimpleProgram::HandleEvent(const SDL_Event* event)
{
	m_imgui.ProcessEvent(event);
}

void SimpleProgram::Frame()
{
	m_sdl.GetSize(m_w, m_h);
	m_vulkan.RebuildSwapchainIfNeeded(m_w, m_h);
	if (m_vulkan.SwapChainRebuild)
		return;

	m_imgui.NewFrame();
	simpleWindow.Draw();
	if (!simpleWindow.IsOpen())
		m_done = true;
	m_imgui.Render();

	ImDrawData* draw_data = ImGui::GetDrawData();
	if (draw_data->DisplaySize.x > 0.0f && draw_data->DisplaySize.y > 0.0f) {
		m_vulkan.SetClearColor({ 0.10f, 0.12f, 0.16f, 1.0f });
		m_vulkan.FrameRender(draw_data);
		m_vulkan.FramePresent();
	}
}

void SimpleProgram::Shutdown()
{
	m_vulkan.WaitIdle();
	m_imgui.Shutdown();
	m_vulkan.CleanupWindow();
	m_vulkan.Cleanup();
	m_sdl.Shutdown();
}

int SimpleProgram::Run()
{
	if (!Init())
		return EXIT_FAILURE;

	while (!m_done) {
		m_sdl.PollEvents(m_done, [this](const SDL_Event* event) {
			HandleEvent(event);
		});

		if (m_sdl.IsMinimized()) {
			SDL_Delay(10);
			continue;
		}

		Frame();
	}

	Shutdown();
	return EXIT_SUCCESS;
}

} // namespace

int main()
{
	SimpleProgram program;
	return program.Run();
}
