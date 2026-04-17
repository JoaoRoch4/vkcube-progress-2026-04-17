#include "app/ui/app_windows.hpp"

#include "app/hot_loader.hpp"
#include "app/renderer/vulkan/cube_renderer.hpp"
#include "app/ui/style_editor.hpp"
#include "app/ui/test_engine_layer.hpp"
#include "app/ui/ui_windows.hpp"
#include "imgui.h"

AppWindows::AppWindows()
    : show_controls { true }
    , show_copilot { true }
    , show_hot_module { true }
    , show_cube { true }
    , CopilotWindowFlags { ImGuiWindowFlags_None }
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
	if (m_apply_copilot_layout_once && m_copilot_window.valid) {
		ImGui::SetNextWindowPos(ImVec2(m_copilot_window.x, m_copilot_window.y), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(m_copilot_window.w, m_copilot_window.h), ImGuiCond_Always);
	} else {
		ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(ImVec2(20, 200), ImGuiCond_Appearing);
	}
	if (ImGui::Begin("Copilot", &show_copilot, CopilotWindowFlags)) {
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		m_copilot_window = { true, pos.x, pos.y, size.x, size.y };
		m_apply_copilot_layout_once = false;

		for (const auto& msg : copilot_messages)
			ImGui::TextWrapped("%s", msg.c_str());
		if (!copilot_messages.empty() && ImGui::Button("Clear"))
			copilot_messages.clear();
		ImGui::Separator();
		ImGui::SetNextItemWidth(-80.0f);
		bool submitted = ImGui::InputText("##copilot_in",
			m_input_buf.data(), m_input_buf.size(),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		if ((ImGui::Button("Send") || submitted) && m_input_buf.at(0) != '\0') {
			user_messages.emplace_back(m_input_buf.data());
			m_input_buf.fill('\0');
			ImGui::SetKeyboardFocusHere(-1);
		}
	}
	ImGui::End();
}

void AppWindows::BuildAll()
{
	BuildMenuBar();
	BuildCopilot();
	m_test_engine->BuildUI(&m_ui_windows->ShowTestEngineWindow);
	if (show_hot_module)
		m_hot->build_ui(&show_hot_module);
}
