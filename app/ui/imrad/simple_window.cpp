// Generated with ImRAD 0.10-WIP
// visit https://github.com/tpecholt/imrad

#include "simple_window.h"

SimpleWindow simpleWindow;

SimpleWindow::SimpleWindow()
    : isOpen { true }
{
}

void SimpleWindow::Open()
{
	isOpen = true;
}

void SimpleWindow::Close()
{
	isOpen = false;
}

void SimpleWindow::Draw()
{
	/// @dpi-info 96,1
	/// @style Dark
	/// @unit px
	/// @begin TopWindow
	if (isOpen) {
		ImGui::SetNextWindowSize({ 420, 220 }, ImGuiCond_FirstUseEver); //{ 420, 220 }
		ImGui::SetNextWindowSizeConstraints({ 0, 0 }, { FLT_MAX, FLT_MAX });
		if (ImGui::Begin("Simple window###SimpleWindow", &isOpen, ImGuiWindowFlags_None)) {
			DrawPopups();

			/// @separator

			/// @begin Text
			ImGui::TextUnformatted("Hello from a minimal ImRAD window.");
			/// @end Text

			/// @begin Button
			if (ImGui::Button("Close")) {
				Close();
			}
			/// @end Button

			/// @separator
		}
		ImGui::End();
	}
	/// @end TopWindow
}

bool SimpleWindow::IsOpen() const
{
	return isOpen;
}

void SimpleWindow::DrawPopups()
{
	// TODO: Draw dependent popups here
}
