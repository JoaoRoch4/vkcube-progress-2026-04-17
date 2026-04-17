#include "app/ui/main_menu_bar.hpp"

#include "app/ui/style_editor.hpp"
#include "app/ui/ui_windows.hpp"
#include "imgui.h"

MainMenuBar::MainMenuBar()
    : m_show_controls { nullptr }
    , m_show_copilot { nullptr }
    , m_show_hot_module { nullptr }
    , m_ui_windows { nullptr }
    , m_style_editor { nullptr }
{
}

void MainMenuBar::Setup(bool* show_controls, bool* show_copilot, bool* show_hot_module,
	UiWindows* ui_windows, StyleEditor* style_editor)
{
	m_show_controls = show_controls;
	m_show_copilot = show_copilot;
	m_show_hot_module = show_hot_module;
	m_ui_windows = ui_windows;
	m_style_editor = style_editor;
}

void MainMenuBar::Build()
{
	if (!m_ui_windows)
		return;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			ImGui::MenuItem("(demo menu)", nullptr, false, false);
			ImGui::MenuItem("New");
			ImGui::MenuItem("Open", "Ctrl+O");
			ImGui::MenuItem("Save", "Ctrl+S");
			ImGui::Separator();
			if (ImGui::MenuItem("Quit", "Alt+F4"))
				m_ui_windows->RequestQuit = true;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			ImGui::MenuItem("Undo", "Ctrl+Z");
			ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
			ImGui::Separator();
			ImGui::MenuItem("Cut", "Ctrl+X");
			ImGui::MenuItem("Copy", "Ctrl+C");
			ImGui::MenuItem("Paste", "Ctrl+V");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			if (m_show_controls)
				ImGui::MenuItem("Controls", nullptr, m_show_controls);
			if (m_show_copilot)
				ImGui::MenuItem("Copilot", nullptr, m_show_copilot);
			if (m_show_hot_module)
				ImGui::MenuItem("Hot Module", nullptr, m_show_hot_module);

			ImGui::Separator();
			ImGui::MenuItem("Hello World", nullptr, &m_ui_windows->ShowHelloWorldWindow);
			ImGui::MenuItem("Style Editor", nullptr, m_style_editor ? &m_style_editor->IsOpen : nullptr);
			ImGui::MenuItem("Demo Window", nullptr, &m_ui_windows->ShowDemoWindow);
			ImGui::MenuItem("Another Window", nullptr, &m_ui_windows->ShowAnotherWindow);
			ImGui::MenuItem("Debug Log Mirror", nullptr, &m_ui_windows->ShowDebugLogMirrorWindow);
			ImGui::MenuItem("Terminal", nullptr, &m_ui_windows->ShowTerminalWindow);
			ImGui::MenuItem("Emoji Atlas", nullptr, &m_ui_windows->ShowEmojiAtlasWindow);
			ImGui::MenuItem("Test Engine", nullptr, &m_ui_windows->ShowTestEngineWindow);
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}
