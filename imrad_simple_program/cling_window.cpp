// Generated with ImRAD 0.10-WIP
// visit https://github.com/tpecholt/imrad

#include "cling_window.h"

#include <format>

ClingWindow clingWindow;

ClingWindow::ClingWindow()
    : isOpen { true }
    , m_input_buf {}
    , m_output {}
    , m_cling {}
    , m_cling_available { ClingRuntime::IsAvailable() }
{
	if (!m_cling_available)
		m_output.emplace_back(
			"[warn] cling not found. Install it or set CLING_BIN=/path/to/cling.");
}

void ClingWindow::Open()
{
	isOpen = true;
}

void ClingWindow::Close()
{
	isOpen = false;
}

void ClingWindow::RunSnippet()
{
	const std::string_view snippet { m_input_buf.data() };
	if (snippet.empty())
		return;

	m_output.emplace_back(std::format(">>> {}", snippet));

	if (!m_cling_available) {
		m_output.emplace_back("[error] cling not available");
		return;
	}

	const ClingRunResult result = m_cling.Execute(snippet);
	m_output.emplace_back(std::format("[{}] {}", result.exit_code, result.output));
	m_input_buf.fill('\0');
}

void ClingWindow::Draw()
{
	/// @dpi-info 96,1
	/// @style Dark
	/// @unit px
	/// @begin TopWindow
	if (isOpen) {
		ImGui::SetNextWindowSize({ 560, 400 }, ImGuiCond_FirstUseEver); //{ 560, 400 }
		ImGui::SetNextWindowSizeConstraints({ 200, 160 }, { FLT_MAX, FLT_MAX });
		if (ImGui::Begin("Cling C++ Interpreter###ClingWindow", &isOpen, ImGuiWindowFlags_None)) {
			DrawPopups();

			/// @separator

			// ── Status banner ──────────────────────────────────────────────
			/// @begin Text
			if (m_cling_available) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.90f, 0.28f, 1.0f));
				ImGui::TextUnformatted("Cling C++ interpreter ready.");
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.40f, 0.40f, 1.0f));
				ImGui::TextUnformatted("Cling not found. Set CLING_BIN or install cling.");
			}
			ImGui::PopStyleColor();
			/// @end Text

			ImGui::Separator();

			// ── Output scroll area ─────────────────────────────────────────
			/// @begin ChildWindow
			const float output_h = ImGui::GetContentRegionAvail().y
			                     - ImGui::GetFrameHeightWithSpacing() * 2.0f - 8.0f;
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
			if (ImGui::BeginChild("##cling_out", ImVec2(0, output_h), ImGuiChildFlags_None,
				    ImGuiWindowFlags_HorizontalScrollbar)) {
				for (const std::string& line : m_output) {
					ImVec4 col;
					if (line.rfind(">>>", 0) == 0)
						col = { 0.48f, 0.90f, 0.48f, 1.0f };
					else if (line.rfind("[error]", 0) == 0 || line.rfind("[warn]", 0) == 0)
						col = { 1.0f, 0.42f, 0.42f, 1.0f };
					else
						col = { 0.88f, 0.88f, 0.92f, 1.0f };
					ImGui::PushStyleColor(ImGuiCol_Text, col);
					ImGui::TextUnformatted(line.c_str());
					ImGui::PopStyleColor();
				}
				if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
					ImGui::SetScrollHereY(1.0f);
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
			/// @end ChildWindow

			ImGui::Separator();

			// ── Input row ─────────────────────────────────────────────────
			/// @begin InputText
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
			const bool enter_pressed = ImGui::InputText("##cling_in", m_input_buf.data(),
				m_input_buf.size(),
				ImGuiInputTextFlags_EnterReturnsTrue);
			/// @end InputText

			ImGui::SameLine();

			/// @begin Button
			if (ImGui::Button("Run") || enter_pressed)
				RunSnippet();
			/// @end Button

			/// @separator
		}
		ImGui::End();
	}
	/// @end TopWindow
}

bool ClingWindow::IsOpen() const
{
	return isOpen;
}

void ClingWindow::DrawPopups()
{
	// TODO: Draw dependent popups here
}
