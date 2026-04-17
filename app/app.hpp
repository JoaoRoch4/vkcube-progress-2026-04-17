#pragma once

#include "app/hot_loader.hpp"
#include "app/platform/sdl_window.hpp"
#include "app/renderer/vulkan/cube_renderer.hpp"
#include "app/renderer/vulkan/vulkan_context_with_cube.hpp"
#include "app/ui/app_windows.hpp"
#include "app/ui/imgui_layer.hpp"
#include "app/ui/style_editor.hpp"
#include "app/ui/test_engine_layer.hpp"
#include "app/ui/ui_windows.hpp"

// App owns all subsystems and drives the main loop.
// Construct one instance and call Run().
class App {
    public:
	App();
	int Run();

    private:
	bool Init();
	void Shutdown();
	void HandleEvent(const SDL_Event* e);
	void Frame();

	SDLWindow m_sdl;
	VulkanContextWithCube m_vulkan;
	CubeRenderer m_cube;
	ImGuiLayer m_imgui;
	TestEngineLayer m_test_engine;
	StyleEditor m_style_editor;
	UiWindows m_ui_windows;
	HotModule m_hot;
	AppWindows m_windows;

	bool m_done;
	bool m_rmb_held;
	float m_prev_mouse_x;
	float m_prev_mouse_y;
	int m_w;
	int m_h;
};
