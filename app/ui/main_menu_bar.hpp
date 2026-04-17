#pragma once

class UiWindows;
class StyleEditor;

// Dedicated owner for the app main menu bar. Inspired by Dear ImGui demo
// (File/Edit menus) and extended with project window toggles.
class MainMenuBar {
    public:
	MainMenuBar();

	void Setup(bool* show_controls, bool* show_copilot, bool* show_hot_module,
		UiWindows* ui_windows, StyleEditor* style_editor);
	void Build();

    private:
	bool* m_show_controls;
	bool* m_show_copilot;
	bool* m_show_hot_module;
	UiWindows* m_ui_windows;
	StyleEditor* m_style_editor;
};
