#include "hot_module.h"
#include "imgui.h"
#include <print>

// ── Hot module ────────────────────────────────────────────────────────────────
//
// This file is compiled into libhot.so separately from the main executable.
// Edit it, run cmake --build cmake-build -j8 (just this target rebuilds in
// seconds), and the running app will pick up the new code on the next frame.
//
// The module shares ImGui state with the main exe via ImGui::SetCurrentContext()
// called in hot_init().  All ImGui API calls work normally here.
//
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// State that survives a hot-reload cycle must live outside this module
// (e.g. in the main exe).  Local state here is reset on every reload.
int s_counter = 0;

void hot_init_fn(ImGuiContext* ctx)
{
	ImGui::SetCurrentContext(ctx);
	s_counter = 0;
	std::println("[hot] module loaded");
}

void hot_build_ui_fn(bool* p_open)
{
	ImGui::SetNextWindowSize(ImVec2(360, 160), ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(ImVec2(540, 380), ImGuiCond_Appearing);
	if (ImGui::Begin("Hot Module [v2] \xf0\x9f\x94\xa5", p_open)) {
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Hot reload works!");
		ImGui::Separator();
		ImGui::Text("Counter: %d", s_counter);
		ImGui::SameLine();
		if (ImGui::Button("+1"))
			++s_counter;
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
			s_counter = 0;
	}
	ImGui::End();
}

void hot_shutdown_fn()
{
	std::println("[hot] module unloading");
}

HotModuleAPI s_api = { hot_init_fn, hot_build_ui_fn, hot_shutdown_fn };

} // namespace

extern "C" HotModuleAPI* hot_get_api()
{
	return &s_api;
}
