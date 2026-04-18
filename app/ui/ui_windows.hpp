#pragma once

#include "imgui.h"
#include "imgui_console.hpp"
#include "imgui_debug_log_mirror.hpp"
#include "imgui_window.hpp"
#include "window_state_toml.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class EmojiAtlas;
class StyleEditor;

// UiWindows owns all Dear ImGui window visibility state and draw calls,
// except the style editor (owned by StyleEditor).
class UiWindows {
    public:
	UiWindows();

	bool ShowDemoWindow;
	bool ShowHelloWorldWindow;
	bool ShowAnotherWindow;
	bool ShowDebugLogMirrorWindow;
	bool ShowTerminalWindow;
	bool ShowTestEngineWindow;
	bool ShowEmojiAtlasWindow;
	bool RequestQuit;
	ImVec4 ClearColor;
	ImGuiWindow HelloWorldWindow;
	ImGuiWindow AnotherWindow;
	ImGuiWindow TerminalWindow;
	ImGuiWindow EmojiAtlasWindow;

	// Realtime file mirror of the Dear ImGui debug log (tail -f friendly).
	ImGuiDebugLogMirror DebugLogMirror;

	// Terminal tabs — each entry owns an independent ConsoleCommands instance.
	struct TerminalTab {
		TerminalTab();
		std::string name;
		std::unique_ptr<ConsoleCommands> console;
		bool open;
	};
	std::vector<TerminalTab> Terminals;

	// Append a new terminal tab.  If name is null, auto-generates "Terminal N".
	void AddTerminal(const char* name = nullptr);

	// Set the emoji atlas for terminals and the emoji atlas window.
	void SetEmojiAtlas(const EmojiAtlas* atlas);

	// Optional controls section rendered inline inside the Hello, world! window.
	void SetHelloWorldControls(bool* show_controls, std::function<void()> draw_controls_section);

	// Must be called after ImGui::CreateContext(): opens debug log, creates Terminal 1.
	void Init();

	// Close debug log (call before ImGuiLayer::Shutdown).
	void Shutdown();

	// Draw all owned windows this frame (includes DebugLogMirror.Tick).
	void Draw(StyleEditor* style_editor);

	// Restore / capture persisted layout.
	void ApplyLayout(const WindowStateToml& state);
	void ExportLayout(WindowStateToml* state) const;

    private:
	void WireTerminalCallbacks(ConsoleCommands& c);
	void DrawTerminals();
	void DrawEmojiAtlasWindow();

	const EmojiAtlas* m_emoji_atlas_view;
	WindowStateToml::WindowRectToml m_hello_world_window;
	WindowStateToml::WindowRectToml m_terminals_window;
	WindowStateToml::WindowRectToml m_emoji_atlas_window;
	WindowStateToml::WindowRectToml m_another_window;
	bool m_apply_hello_world_layout_once;
	bool m_apply_terminals_layout_once;
	bool m_apply_emoji_atlas_layout_once;
	bool m_apply_another_layout_once;
	bool* m_show_controls;
	std::function<void()> m_draw_controls_section;
};
