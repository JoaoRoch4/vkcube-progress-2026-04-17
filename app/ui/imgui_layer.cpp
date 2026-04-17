#include "imgui_layer.hpp"
#include "imgui_freetype.h"

#include "imgui.h"

void ImGuiLayer::Init(SDL_Window* window, ImGui_ImplVulkan_InitInfo& init_info, float main_scale)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();

	// Bake a fixed style / DPI scale (see docs/FONTS.md for dynamic per-monitor scaling).
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;

	ImGui_ImplSDL3_InitForVulkan(window);
	ImGui_ImplVulkan_Init(&init_info);

	// Load fonts — see docs/FONTS.md.
	// Using 1.92 dynamic font system (no glyph ranges required).
	style.FontSizeBase = 20.0f;

	// 1. Primary: NotoSans — solid Latin, Greek, Cyrillic, Arabic, Hebrew …
	io.Fonts->AddFontFromFileTTF("/usr/share/fonts/google-noto/NotoSans-Regular.ttf");

	// 2. Merge CJK (Chinese, Japanese, Korean) into the same font slot.
	{
		ImFontConfig cfg;
		cfg.MergeMode = true;
		io.Fonts->AddFontFromFileTTF(
			"/home/joao/.local/share/fonts/google/NotoSansSC[wght].ttf",
			0.0f, &cfg);
	}

	// 3. Merge Arabic (covers Arabic script + Arabic Presentation Forms U+FE70-U+FEFF).
	{
		ImFontConfig cfg;
		cfg.MergeMode = true;
		io.Fonts->AddFontFromFileTTF(
			"/home/joao/.local/share/fonts/google/NotoSansArabic[wdth,wght].ttf",
			0.0f, &cfg);
	}

	// 4. Merge Mathematical symbols.
	{
		ImFontConfig cfg;
		cfg.MergeMode = true;
		io.Fonts->AddFontFromFileTTF(
			"/home/joao/.local/share/fonts/google/NotoSansMath-Regular.ttf",
			0.0f, &cfg);
	}

	// 5. Merge Symbols & Symbols 2.
	{
		ImFontConfig cfg;
		cfg.MergeMode = true;
		io.Fonts->AddFontFromFileTTF(
			"/home/joao/.local/share/fonts/google/NotoSansSymbols2-Regular.ttf",
			0.0f, &cfg);
	}

	// 6. Merge color emoji.
	{
		ImFontConfig cfg;
		cfg.MergeMode = true;
		cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LoadColor;
		io.Fonts->AddFontFromFileTTF(
			"/home/joao/.local/share/fonts/google/NotoColorEmoji-Regular.ttf",
			0.0f, &cfg);
	}
}

void ImGuiLayer::Shutdown()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::ProcessEvent(const SDL_Event* event)
{
	ImGui_ImplSDL3_ProcessEvent(event);
}

void ImGuiLayer::NewFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::Render()
{
	ImGui::Render();
}

