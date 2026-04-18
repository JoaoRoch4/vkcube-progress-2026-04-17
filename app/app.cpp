#include "app.hpp"

#include <cstdlib>
#include <filesystem>
#include <print>
#include <string>

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_te_engine.h"

#include "app/ui/window_state_toml.hpp"

// ── File-scope pointers for extern "C" LLDB hooks ─────────────────────────────
// Set during App::Init, cleared during App::Shutdown.
static AppWindows* g_windows = nullptr;
static TestEngineLayer* g_test_engine = nullptr;
static HotModule* g_hot_module = nullptr;

static std::filesystem::path WindowStateFilePath()
{
	return std::filesystem::current_path() / "window_state.toml";
}

// ── Copilot ↔ App messaging ────────────────────────────────────────────────────
// Copilot → App: call copilot_say("message") from LLDB evaluate()
// App → Copilot: call copilot_read() from LLDB evaluate() to pop next user msg
// Test control:  call copilot_run_test("category/name") to queue a test

extern "C" void copilot_say(const char* msg)
{
	if (msg && g_windows)
		g_windows->copilot_messages.emplace_back(msg);
}

extern "C" const char* copilot_read()
{
	static std::string s_buf;
	if (!g_windows || g_windows->user_messages.empty())
		return nullptr;
	s_buf = std::move(g_windows->user_messages.front());
	g_windows->user_messages.pop_front();
	return s_buf.c_str();
}

extern "C" void copilot_run_test(const char* filter)
{
	if (g_test_engine && g_test_engine->Engine && filter)
		ImGuiTestEngine_QueueTests(g_test_engine->Engine,
			ImGuiTestGroup_Tests, filter, ImGuiTestRunFlags_None);
}

extern "C" void copilot_reload_hot()
{
	if (g_hot_module)
		g_hot_module->load();
}

extern "C" void copilot_run_js(const char* msg)
{
	if (msg && g_windows)
		g_windows->RunJavaScript(msg);
}

extern "C" void copilot_run_python(const char* msg)
{
	if (msg && g_windows)
		g_windows->RunPython(msg);
}

// ─────────────────────────────────────────────────────────────────────────────

App::App()
    : m_done { false }
    , m_rmb_held { false }
    , m_prev_mouse_x { 0.0f }
    , m_prev_mouse_y { 0.0f }
    , m_w { 0 }
    , m_h { 0 }
{
}

bool App::Init()
{
	// 1. Window
	if (!m_sdl.Init("vkcube + ImGui", 1280, 800)) {
		std::println(stderr, "SDLWindow::Init failed");
		return false;
	}

	// 2. Vulkan (subclass with cube injection)
	m_vulkan.Setup(m_sdl.GetVulkanExtensions());
	VkSurfaceKHR surface = m_sdl.CreateVulkanSurface(m_vulkan.Instance, m_vulkan.Allocator);
	if (surface == VK_NULL_HANDLE) {
		std::println(stderr, "CreateVulkanSurface failed");
		return false;
	}
	m_sdl.GetSize(m_w, m_h);
	m_vulkan.SetupWindow(surface, m_w, m_h);
	m_sdl.Show();

	// 3. Cube renderer — uses the same device/renderpass as ImGui Vulkan backend
	VkPhysicalDeviceMemoryProperties mem_props {};
	vkGetPhysicalDeviceMemoryProperties(m_vulkan.PhysicalDevice, &mem_props);
	m_cube.Init(m_vulkan.Device,
		m_vulkan.PhysicalDevice,
		mem_props,
		m_vulkan.Queue,
		m_vulkan.QueueFamily,
		m_vulkan.MainWindowData.RenderPass,
		m_vulkan.DescriptorPool);
	m_vulkan.SetCubeRenderer(&m_cube, &m_windows.show_cube);

	// 4. ImGui
	ImGui_ImplVulkan_InitInfo init_info = m_vulkan.MakeInitInfo();
	m_imgui.Init(m_sdl.Window, init_info, m_sdl.MainScale);

	// 5. Style defaults and UI windows (must be after ImGui::CreateContext)
	m_style_editor.InitDefaults();
	m_ui_windows.Init();

	// 6. Test engine (must be after ImGui::CreateContext)
	m_test_engine.Init();
	g_test_engine = &m_test_engine;

	// Wire Vulkan readback to test engine screen capture
	{
		ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(m_test_engine.Engine);
		test_io.ScreenCaptureFunc = [](ImGuiID, int x, int y, int w, int h,
						    unsigned int* pixels, void* user_data) -> bool {
			return static_cast<VulkanContext*>(user_data)->ReadPixels(x, y, w, h, pixels);
		};
		test_io.ScreenCaptureUserData = &m_vulkan;
	}

	// 6. Hot-reload module (initial load — silently skipped if libhot.so absent)
	g_hot_module = &m_hot;
	m_hot.load();

	// 7. Wire up UI windows and expose to LLDB hooks
	m_windows.Setup(&m_cube, &m_hot, &m_ui_windows, &m_style_editor, &m_test_engine);
	g_windows = &m_windows;

	// 8. Restore persisted window visibility state (TOML)
	WindowStateToml persisted_state {};
	if (LoadWindowStateToml(WindowStateFilePath(), persisted_state)) {
		m_windows.show_controls = persisted_state.show_controls;
		m_windows.show_copilot = persisted_state.show_copilot;
		m_windows.show_hot_module = persisted_state.show_hot_module;
		m_windows.show_cube = persisted_state.show_cube;
		if (persisted_state.clear_color.has_value()) {
			const WindowStateToml::Vec4Toml& color = persisted_state.clear_color.value();
			m_ui_windows.ClearColor = { color.x, color.y, color.z, color.w };
		}
		if (persisted_state.cube_auto_spin.has_value())
			m_cube.SetAnimate(persisted_state.cube_auto_spin.value());

		m_style_editor.IsOpen = persisted_state.show_style_editor_window;
		m_ui_windows.ShowDemoWindow = persisted_state.show_demo_window;
		m_ui_windows.ShowAnotherWindow = persisted_state.show_another_window;
		m_ui_windows.ShowDebugLogMirrorWindow = persisted_state.show_debug_log_mirror_window;
		m_ui_windows.ShowTerminalWindow = persisted_state.show_terminal_window;
		m_ui_windows.ShowTestEngineWindow = persisted_state.show_test_engine_window;
		m_ui_windows.ShowEmojiAtlasWindow = persisted_state.show_emoji_atlas_window;

		m_windows.ApplyLayout(persisted_state);
		m_style_editor.ApplyLayout(persisted_state);
		m_ui_windows.ApplyLayout(persisted_state);
	}

	return true;
}

void App::HandleEvent(const SDL_Event* e)
{
	m_imgui.ProcessEvent(e);

	if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_RIGHT) {
		m_rmb_held = true;
		m_prev_mouse_x = e->button.x;
		m_prev_mouse_y = e->button.y;
	}
	if (e->type == SDL_EVENT_MOUSE_BUTTON_UP && e->button.button == SDL_BUTTON_RIGHT) {
		m_rmb_held = false;
	}
	if (e->type == SDL_EVENT_MOUSE_MOTION && m_rmb_held) {
		m_cube.AddMouseDelta(e->motion.x - m_prev_mouse_x,
			e->motion.y - m_prev_mouse_y);
		m_prev_mouse_x = e->motion.x;
		m_prev_mouse_y = e->motion.y;
	}
	if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_MIDDLE) {
		m_cube.ResetMouseRotation();
	}
	if (e->type == SDL_EVENT_MOUSE_WHEEL && m_windows.show_cube) {
		// ImGui may have captured the wheel; only scroll the cube when
		// the cursor is not over a Dear ImGui window.
		if (!ImGui::GetIO().WantCaptureMouse)
			m_cube.AddScrollDelta(e->wheel.y);
	}
}

void App::Frame()
{
	m_sdl.GetSize(m_w, m_h);
	m_vulkan.RebuildSwapchainIfNeeded(m_w, m_h);
	if (m_vulkan.SwapChainRebuild)
		return;

	m_imgui.NewFrame();
	m_hot.tick();

	m_ui_windows.Draw(&m_style_editor);
	m_windows.BuildAll();
	m_imgui.Render();

	ImDrawData* draw_data = ImGui::GetDrawData();
	if (draw_data->DisplaySize.x > 0.0f && draw_data->DisplaySize.y > 0.0f) {
		if (!m_windows.show_cube)
			m_vulkan.SetClearColor(m_ui_windows.ClearColor);
		else
			m_vulkan.SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		m_vulkan.FrameRender(draw_data);
		m_vulkan.FramePresent();
		m_test_engine.PostSwap();
	}
}

int App::Run()
{
	if (!Init())
		return EXIT_FAILURE;

	while (!m_done) {
		m_sdl.PollEvents(m_done, [this](const SDL_Event* e) {
			HandleEvent(e);
		});

		m_done = m_done || m_ui_windows.RequestQuit;
		if (m_sdl.IsMinimized()) {
			SDL_Delay(10);
			continue;
		}

		Frame();
	}

	Shutdown();
	return EXIT_SUCCESS;
}

void App::Shutdown()
{
	WindowStateToml persisted_state {};
	persisted_state.show_controls = m_windows.show_controls;
	persisted_state.show_copilot = m_windows.show_copilot;
	persisted_state.show_hot_module = m_windows.show_hot_module;
	persisted_state.show_cube = m_windows.show_cube;
	persisted_state.clear_color = WindowStateToml::Vec4Toml {
		m_ui_windows.ClearColor.x,
		m_ui_windows.ClearColor.y,
		m_ui_windows.ClearColor.z,
		m_ui_windows.ClearColor.w,
	};
	persisted_state.cube_auto_spin = m_cube.GetAnimate();
	persisted_state.show_style_editor_window = m_style_editor.IsOpen;
	persisted_state.show_demo_window = m_ui_windows.ShowDemoWindow;
	persisted_state.show_another_window = m_ui_windows.ShowAnotherWindow;
	persisted_state.show_debug_log_mirror_window = m_ui_windows.ShowDebugLogMirrorWindow;
	persisted_state.show_terminal_window = m_ui_windows.ShowTerminalWindow;
	persisted_state.show_test_engine_window = m_ui_windows.ShowTestEngineWindow;
	persisted_state.show_emoji_atlas_window = m_ui_windows.ShowEmojiAtlasWindow;
	m_windows.ExportLayout(&persisted_state);
	m_style_editor.ExportLayout(&persisted_state);
	m_ui_windows.ExportLayout(&persisted_state);
	if (!SaveWindowStateToml(WindowStateFilePath(), persisted_state))
		std::println(stderr, "Failed to save window state TOML to {}", WindowStateFilePath().string());

	m_vulkan.WaitIdle();
	m_cube.Cleanup();
	m_hot.shutdown();
	g_hot_module = nullptr;
	m_test_engine.Stop();
	m_ui_windows.Shutdown();
	m_imgui.Shutdown();
	m_test_engine.Shutdown();
	g_test_engine = nullptr;
	g_windows = nullptr;
	m_vulkan.CleanupWindow();
	m_vulkan.Cleanup();
	m_sdl.Shutdown();
}
