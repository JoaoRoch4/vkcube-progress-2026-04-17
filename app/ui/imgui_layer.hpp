#pragma once

#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include <SDL3/SDL.h>

// ImGuiLayer owns the Dear ImGui context and both backends.
// UI window state lives in UiWindows and StyleEditor.
class ImGuiLayer {
    public:
	// Create the ImGui context, configure IO flags, apply style, init both backends,
	// load fonts.  init_info must be fully populated (see VulkanContext::MakeInitInfo).
	void Init(SDL_Window* window, ImGui_ImplVulkan_InitInfo& init_info, float main_scale);

	// Shutdown both backends and destroy the ImGui context.
	void Shutdown();

	// Forward an SDL event to the SDL3 backend (call for every polled event).
	void ProcessEvent(const SDL_Event* event);

	// Call ImGui_ImplVulkan_NewFrame + ImGui_ImplSDL3_NewFrame + ImGui::NewFrame.
	void NewFrame();

	// Call ImGui::Render() to finalise draw data.
	void Render();
};
