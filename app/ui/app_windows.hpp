#pragma once

#include <array>
#include <deque>
#include <string>
#include <string_view>

#include "app/ui/imgui_window.hpp"
#include "scripting/hybrid_runtime.hpp"
#include "app/ui/main_menu_bar.hpp"
#include "app/ui/window_state_toml.hpp"

class CubeRenderer;
struct HotModule;
class UiWindows;
class StyleEditor;
class TestEngineLayer;

// AppWindows owns all per-frame Dear ImGui window draw calls and the visibility
// state for each window.  Also owns the Copilot messaging queues so that
// extern "C" LLDB hooks can reach them via the App instance.
class AppWindows {
    public:
	AppWindows();

	bool show_controls;
	bool show_copilot;
	bool show_hot_module;
	bool show_cube;
	ImGuiWindow CopilotWindow;

	std::deque<std::string> copilot_messages; // inbound  (Copilot → App)
	std::deque<std::string> user_messages; // outbound (App → Copilot)

	void Setup(CubeRenderer* cube, HotModule* hot, UiWindows* ui_windows,
		StyleEditor* style_editor, TestEngineLayer* test_engine);
	void RunJavaScript(std::string_view prompt);
	void RunPython(std::string_view prompt);
	void ApplyLayout(const WindowStateToml& state);
	void ExportLayout(WindowStateToml* state) const;
	void BuildAll();

    private:
	void BuildMenuBar();
	void DrawControlsSection();
	void BuildCopilot();

	CubeRenderer* m_cube;
	HotModule* m_hot;
	UiWindows* m_ui_windows;
	StyleEditor* m_style_editor;
	TestEngineLayer* m_test_engine;
	std::array<char, 512> m_input_buf;
	MainMenuBar m_main_menu;
	HybridScriptRuntime m_hybrid_runtime;
	WindowStateToml::WindowRectToml m_copilot_window;
	bool m_apply_copilot_layout_once;
};
