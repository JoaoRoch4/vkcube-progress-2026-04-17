#pragma once

#include "imgui.h"
#include "window_state_toml.hpp"
#include <string>

// StyleEditor owns the Dear ImGui style preset selection, preset switching
// logic, and the full ImGuiStyle TOML persistence.
class StyleEditor {
    public:
	StyleEditor();
	bool IsOpen;
	ImGuiWindowFlags WindowFlags;

	// Call immediately after ImGuiLayer::Init to capture the default style.
	void InitDefaults();

	// Draw the style editor window (no-op when IsOpen == false).
	void Draw();

	// Restore / capture persisted layout.
	void ApplyLayout(const WindowStateToml& state);
	void ExportLayout(WindowStateToml* state) const;

    private:
	void ApplyPresetByName(const std::string& preset_name);

	ImGuiStyle m_default_style;
	std::string m_current_preset_name;
	WindowStateToml::WindowRectToml m_window_rect;
	bool m_apply_layout_once;
};
