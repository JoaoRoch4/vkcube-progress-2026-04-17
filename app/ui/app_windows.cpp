#include "app_windows.hpp"

#include "app/hot_loader.hpp"
#include "app/renderer/vulkan/cube_renderer.hpp"
#include "app/ui/imanim.hpp"
#include "app/ui/style_editor.hpp"
#include "app/ui/test_engine_layer.hpp"
#include "app/ui/ui_windows.hpp"
#include "imgui.h"
#include <filesystem>
#include <format>

AppWindows::AppWindows()
    : show_controls { true }
    , show_copilot { true }
    , show_hot_module { true }
    , show_cube { true }
    , CopilotWindow {}
    , copilot_messages {}
    , user_messages {}
    , m_cube { nullptr }
    , m_hot { nullptr }
    , m_ui_windows { nullptr }
    , m_style_editor { nullptr }
    , m_test_engine { nullptr }
    , m_input_buf {}
    , m_copilot_window { false, 0.0f, 0.0f, 0.0f, 0.0f }
    , m_apply_copilot_layout_once { false }
    , m_copilot_waiting { false }
    , m_copilot_prev_msg_count { 0 }
{
}

void AppWindows::Setup(CubeRenderer* cube, HotModule* hot, UiWindows* ui_windows,
	StyleEditor* style_editor, TestEngineLayer* test_engine)
{
	m_cube = cube;
	m_hot = hot;
	m_ui_windows = ui_windows;
	m_style_editor = style_editor;
	m_test_engine = test_engine;
	m_main_menu.Setup(&show_controls, &show_copilot, &show_hot_module, m_ui_windows, m_style_editor);
	m_ui_windows->SetHelloWorldControls(&show_controls, [this]() { DrawControlsSection(); });
	m_hybrid_runtime.Init(std::filesystem::current_path());
}

void AppWindows::RunJavaScript(std::string_view prompt)
{
	const HybridRunResult result = m_hybrid_runtime.RunJavaScript(prompt);
	copilot_messages.emplace_back(std::format("[{}:{}] {}",
		result.runtime,
		result.exit_code,
		result.output));
}

void AppWindows::RunPython(std::string_view prompt)
{
	const HybridRunResult result = m_hybrid_runtime.RunPython(prompt);
	copilot_messages.emplace_back(std::format("[{}:{}] {}",
		result.runtime,
		result.exit_code,
		result.output));
}

void AppWindows::ApplyLayout(const WindowStateToml& state)
{
	m_copilot_window = state.copilot_window;
	m_apply_copilot_layout_once = m_copilot_window.valid;
}

void AppWindows::ExportLayout(WindowStateToml* state) const
{
	if (!state)
		return;
	state->copilot_window = m_copilot_window;
}

void AppWindows::BuildMenuBar()
{
	m_main_menu.Build();
}

void AppWindows::DrawControlsSection()
{
	ImGui::Checkbox("Cube background", &show_cube);
	if (show_cube) {
		bool anim = m_cube->GetAnimate();
		if (ImGui::Checkbox("Auto-spin", &anim))
			m_cube->SetAnimate(anim);
		if (ImGui::Button("Reset rotation"))
			m_cube->ResetMouseRotation();
		ImGui::TextDisabled("Right-drag to rotate, Middle to reset");
	}
	ImGui::Separator();
	ImGui::Text("%.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));
	ImGui::Separator();
	if (ImGui::Button("Reload hot module"))
		m_hot->load();
}

void AppWindows::BuildCopilot()
{
	if (!show_copilot)
		return;

	// ── Apply persisted layout ───────────────────────────────────────────────
	if (m_apply_copilot_layout_once && m_copilot_window.valid) {
		CopilotWindow.SetNextPos(ImVec2(m_copilot_window.x, m_copilot_window.y), ImGuiCond_Always);
		CopilotWindow.SetNextSize(ImVec2(m_copilot_window.w, m_copilot_window.h), ImGuiCond_Always);
	} else {
		CopilotWindow.SetNextSize(ImVec2(620.0f, 460.0f), ImGuiCond_Appearing);
		CopilotWindow.SetNextPos(ImVec2(20.0f, 200.0f), ImGuiCond_Appearing);
	}

	// ── Konsole-style dark terminal color theme ──────────────────────────────
	// Background and title colours mimic the Konsole dark colour scheme.
	ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(0.08f, 0.08f, 0.11f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_TitleBg,        ImVec4(0.16f, 0.16f, 0.24f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  ImVec4(0.20f, 0.22f, 0.46f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,    ImVec4(0.05f, 0.05f, 0.07f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,  ImVec4(0.32f, 0.32f, 0.48f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(0.28f, 0.28f, 0.46f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_ResizeGrip,     ImVec4(0.28f, 0.28f, 0.46f, 0.60f));
	ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, ImVec4(0.40f, 0.40f, 0.70f, 0.80f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

	const bool window_visible = CopilotWindow.Begin(
		"\xe2\x97\x8f  Copilot \xe2\x80\x94 Konsole", // "●  Copilot — Konsole"
		&show_copilot, CopilotWindow.Flags);

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(8);

	if (!window_visible) {
		CopilotWindow.End();
		return;
	}

	// ── Persist window geometry ──────────────────────────────────────────────
	{
		const ImVec2 pos  = CopilotWindow.GetWindowPos();
		const ImVec2 size = CopilotWindow.GetWindowSize();
		m_copilot_window           = { true, pos.x, pos.y, size.x, size.y };
		m_apply_copilot_layout_once = false;
	}

	// ── Detect new responses (clear the waiting spinner) ─────────────────────
	if (m_copilot_waiting && copilot_messages.size() > m_copilot_prev_msg_count)
		m_copilot_waiting = false;
	m_copilot_prev_msg_count = copilot_messages.size();

	const float win_w = ImGui::GetContentRegionAvail().x;

	// ── Konsole-style fake menu bar strip ────────────────────────────────────
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.20f, 1.00f));
	if (ImGui::BeginChild("##kbar", ImVec2(win_w, 22.0f),
		    ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
		ImGui::SetCursorPos(ImVec2(8.0f, 4.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.68f, 0.78f, 1.00f));
		ImGui::TextUnformatted("File  Edit  View  Bookmarks  Settings  Help");
		ImGui::PopStyleColor();
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	// ── Terminal output area ─────────────────────────────────────────────────
	//   Reserve space at the bottom for the spinner row + input row.
	const float line_h    = ImGui::GetTextLineHeightWithSpacing();
	const float spinner_h = m_copilot_waiting ? (line_h + 4.0f) : 0.0f;
	const float input_h   = ImGui::GetFrameHeightWithSpacing() + 4.0f;
	const float sep_h     = 1.0f;
	const float output_h  = ImGui::GetContentRegionAvail().y
	                        - spinner_h - input_h - sep_h;

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.11f, 1.00f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(4.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
	if (ImGui::BeginChild("##term_out", ImVec2(win_w, output_h),
		    ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {

		// Static header — mimics the gh copilot welcome banner.
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.22f, 0.72f, 1.00f, 1.00f));
		ImGui::TextUnformatted(
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
		ImGui::Text("  \xe2\x97\x86 GitHub Copilot"); // "  ◆ GitHub Copilot"
		ImGui::TextUnformatted(
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
			"\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
		ImGui::PopStyleColor();

		// Conversation history: user prompts and copilot responses.
		int item_id = 0;
		for (std::size_t i = 0; i < user_messages.size() || i < copilot_messages.size(); ++i) {
			// User turn
			if (i < user_messages.size()) {
				ImGui::PushID(item_id++);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.90f, 0.28f, 1.00f));
				ImGui::Text("\xe2\x9d\xaf %s", // "❯ <prompt>"
					user_messages.at(i).c_str());
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
			// Copilot response
			if (i < copilot_messages.size()) {
				const std::string& msg = copilot_messages.at(i);
				ImGui::PushID(item_id++);
				ImVec4 text_col;
				if (msg.compare(0, 7, "[error]") == 0 || msg.compare(0, 7, "[warn]") == 0)
					text_col = { 1.00f, 0.38f, 0.38f, 1.00f };
				else if (msg.compare(0, 4, "[js:") == 0 || msg.compare(0, 4, "[py:") == 0)
					text_col = { 0.48f, 0.92f, 0.48f, 1.00f };
				else
					text_col = { 0.88f, 0.88f, 0.92f, 1.00f };
				ImGui::PushStyleColor(ImGuiCol_Text, text_col);
				ImGui::TextWrapped("%s", msg.c_str());
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
		}

		// Keep the log scrolled to the newest content.
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - line_h)
			ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	// ── Spinner row (visible only while waiting for a response) ──────────────
	if (m_copilot_waiting) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
		ImAnim::DrawSpinner("Thinking", ImVec4(1.0f, 0.85f, 0.20f, 1.00f));
	}

	// ── Divider ──────────────────────────────────────────────────────────────
	ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.28f, 0.28f, 0.46f, 1.00f));
	ImGui::Separator();
	ImGui::PopStyleColor();

	// ── Input row — styled like a kconsole prompt ─────────────────────────────
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.14f, 1.00f));
	if (ImGui::BeginChild("##prompt_row", ImVec2(win_w, input_h),
		    ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

		// "copilot" label
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.80f, 0.56f, 1.00f));
		ImGui::TextUnformatted("copilot");
		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 2.0f);

		// "❯" arrow glyph
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.72f, 1.00f));
		ImGui::TextUnformatted("\xe2\x9d\xaf"); // ❯
		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 4.0f);

		// Text input — frameless, blends into the dark bar
		ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.10f, 0.14f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.20f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.14f, 0.14f, 0.20f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.92f, 0.92f, 0.96f, 1.00f));
		ImGui::SetNextItemWidth(-1.0f);
		const bool submitted = ImGui::InputText("##copilot_in",
			m_input_buf.data(), m_input_buf.size(),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopStyleColor(4);

		if (submitted && m_input_buf.at(0) != '\0') {
			const std::string prompt(m_input_buf.data());
			user_messages.emplace_back(prompt);
			m_copilot_prev_msg_count = copilot_messages.size();
			m_copilot_waiting        = true;
			RunJavaScript(prompt);
			m_input_buf.fill('\0');
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	CopilotWindow.End();
}

void AppWindows::BuildAll()
{
	BuildMenuBar();
	BuildCopilot();
	m_test_engine->BuildUI(&m_ui_windows->ShowTestEngineWindow);
	if (show_hot_module)
		m_hot->build_ui(&show_hot_module);
}
