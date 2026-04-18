#include "app/ui/ui_windows.hpp"

#include "app/ui/style_editor.hpp"
#include "emoji_atlas.hpp"
#include "imgui.h"
#include "imgui_window.hpp"

#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>

UiWindows::TerminalTab::TerminalTab()
    : open { true }
{
}

UiWindows::UiWindows()
    : ShowDemoWindow { true }
    , ShowHelloWorldWindow { true }
    , ShowAnotherWindow { false }
    , ShowDebugLogMirrorWindow { true }
    , ShowTerminalWindow { true }
    , ShowTestEngineWindow { true }
    , ShowEmojiAtlasWindow { true }
    , RequestQuit { false }
    , ClearColor { 0.45f, 0.55f, 0.60f, 1.00f }
    , HelloWorldWindow {}
    , AnotherWindow {}
    , TerminalWindow {}
    , EmojiAtlasWindow {}
    , m_emoji_atlas_view { nullptr }
    , m_hello_world_window { false, 0.0f, 0.0f, 0.0f, 0.0f }
    , m_terminals_window { false, 0.0f, 0.0f, 0.0f, 0.0f }
    , m_emoji_atlas_window { false, 0.0f, 0.0f, 0.0f, 0.0f }
    , m_another_window { false, 0.0f, 0.0f, 0.0f, 0.0f }
    , m_apply_hello_world_layout_once { false }
    , m_apply_terminals_layout_once { false }
    , m_apply_emoji_atlas_layout_once { false }
    , m_apply_another_layout_once { false }
    , m_show_controls { nullptr }
    , m_draw_controls_section {}
{
}

void UiWindows::Init()
{
	// Start mirroring the debug log to a file (follow with: tail -f /tmp/imgui_debug.log)
	DebugLogMirror.Open("/tmp/imgui_debug.log");

	// Create the initial terminal tab (wires all callbacks internally).
	AddTerminal("Terminal 1");
}

void UiWindows::Shutdown()
{
	DebugLogMirror.Close();
}

void UiWindows::WireTerminalCallbacks(ConsoleCommands& c)
{
	c.OnDemoToggle = [this](bool show) { ShowDemoWindow = show; };
	c.OnStyleChange = [](int) { /* style already applied in-place by CmdStyle */ };
	c.OnQuit = [this]() { RequestQuit = true; };
}

void UiWindows::AddTerminal(const char* name)
{
	TerminalTab t;
	t.name = name ? name : ("Terminal " + std::to_string(Terminals.size() + 1));
	t.console = std::make_unique<ConsoleCommands>();
	t.console->SetEmojiAtlas(m_emoji_atlas_view);
	WireTerminalCallbacks(*t.console);
	for (const StoredCallable& c : m_callables)
		t.console->RegisterCallable(c.name.c_str(), c.description.c_str(), c.fn);
	Terminals.push_back(std::move(t));
}

void UiWindows::SetEmojiAtlas(const EmojiAtlas* atlas)
{
	m_emoji_atlas_view = atlas;
	for (TerminalTab& terminal : Terminals)
		terminal.console->SetEmojiAtlas(atlas);
}

void UiWindows::RegisterCallable(const char* name, const char* description,
	std::function<void(std::string_view)> fn)
{
	// Register on all existing terminals.
	for (TerminalTab& t : Terminals)
		t.console->RegisterCallable(name, description, fn);
	// Store for terminals created later.
	m_callables.push_back({ name, description, std::move(fn) });
}

void UiWindows::SetHelloWorldControls(bool* show_controls, std::function<void()> draw_controls_section)
{
	m_show_controls = show_controls;
	m_draw_controls_section = std::move(draw_controls_section);
}

void UiWindows::DrawTerminals()
{
	if (m_apply_terminals_layout_once && m_terminals_window.valid) {
		TerminalWindow.SetNextPos(ImVec2(m_terminals_window.x, m_terminals_window.y), ImGuiCond_Always);
		TerminalWindow.SetNextSize(ImVec2(m_terminals_window.w, m_terminals_window.h), ImGuiCond_Always);
	} else {
		TerminalWindow.SetNextSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
	}
	if (!TerminalWindow.Begin("Terminals", &ShowTerminalWindow, TerminalWindow.Flags)) {
		TerminalWindow.End();
		return;
	}
	{
		ImVec2 pos = TerminalWindow.GetWindowPos();
		ImVec2 size = TerminalWindow.GetWindowSize();
		m_terminals_window = { true, pos.x, pos.y, size.x, size.y };
		m_apply_terminals_layout_once = false;
	}
	if (ImGui::BeginTabBar("##termtabs")) {
		for (int i = 0; i < static_cast<int>(Terminals.size());) {
			TerminalTab& t = Terminals.at(i);
			bool open = t.open;
			std::string label = std::format("{}##tab{}", t.name, i);
			if (ImGui::BeginTabItem(label.c_str(), &open)) {
				std::string id = std::format("{}", i);
				t.console->DrawContents(id.c_str());
				ImGui::EndTabItem();
			}
			t.open = open;
			if (!open)
				Terminals.erase(Terminals.begin() + i);
			else
				++i;
		}
		// "+" button appends a new terminal tab.
		if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
			AddTerminal();
		ImGui::EndTabBar();
	}
	TerminalWindow.End();
}

void UiWindows::DrawEmojiAtlasWindow()
{
	if (m_emoji_atlas_view == nullptr)
		return;

	if (m_apply_emoji_atlas_layout_once && m_emoji_atlas_window.valid) {
		EmojiAtlasWindow.SetNextPos(ImVec2(m_emoji_atlas_window.x, m_emoji_atlas_window.y), ImGuiCond_Always);
		EmojiAtlasWindow.SetNextSize(ImVec2(m_emoji_atlas_window.w, m_emoji_atlas_window.h), ImGuiCond_Always);
	} else {
		EmojiAtlasWindow.SetNextSize(ImVec2(420.0f, 260.0f), ImGuiCond_FirstUseEver);
	}
	if (!EmojiAtlasWindow.Begin("Emoji Atlas", &ShowEmojiAtlasWindow, EmojiAtlasWindow.Flags)) {
		EmojiAtlasWindow.End();
		return;
	}
	{
		ImVec2 pos = EmojiAtlasWindow.GetWindowPos();
		ImVec2 size = EmojiAtlasWindow.GetWindowSize();
		m_emoji_atlas_window = { true, pos.x, pos.y, size.x, size.y };
		m_apply_emoji_atlas_layout_once = false;
	}

	ImGui::Text("Merged text path:");
	ImGui::TextUnformatted("\xF0\x9F\x8C\x9F  \xF0\x9F\x9A\x80");
	ImGui::Separator();

	struct EmojiSample {
		ImWchar codepoint;
		const char* label;
	};
	const std::array<EmojiSample, 2> samples { {
		{ static_cast<ImWchar>(0x1F31F), "U+1F31F glowing star" },
		{ static_cast<ImWchar>(0x1F680), "U+1F680 rocket" },
	} };

	for (const EmojiSample& sample : samples) {
		ImGui::TextUnformatted(sample.label);
		const EmojiAtlas::GlyphEntry* glyph = m_emoji_atlas_view->LookupGlyph(sample.codepoint);
		if (glyph == nullptr) {
			ImGui::TextUnformatted("Atlas glyph missing");
			continue;
		}

		const ImVec2 image_size {
			static_cast<float>(glyph->RenderW) * 2.0f,
			static_cast<float>(glyph->RenderH) * 2.0f,
		};
		ImGui::Image(m_emoji_atlas_view->GetTextureRef(), image_size,
			ImVec2(glyph->U0, glyph->V0), ImVec2(glyph->U1, glyph->V1));
		ImGui::SameLine();
		ImGui::Text("%dx%d", glyph->RenderW, glyph->RenderH);
	}

	ImGui::Separator();
	ImGui::Text("Atlas size: %d x %d", m_emoji_atlas_view->AtlasWidth(), m_emoji_atlas_view->AtlasHeight());
	EmojiAtlasWindow.End();
}

void UiWindows::Draw(StyleEditor* style_editor)
{
	DebugLogMirror.Tick(); // append any new debug-log bytes to /tmp/imgui_debug.log

	// 1. Show the big demo window.
	if (ShowDemoWindow)
		ImGui::ShowDemoWindow(&ShowDemoWindow);

	// 2. Show a simple window that we create ourselves.
	if (ShowHelloWorldWindow) {
		static float f = 0.0f;
		static int counter = 0;

		if (m_apply_hello_world_layout_once && m_hello_world_window.valid) {
			HelloWorldWindow.SetNextPos(ImVec2(m_hello_world_window.x, m_hello_world_window.y), ImGuiCond_Always);
			HelloWorldWindow.SetNextSize(ImVec2(m_hello_world_window.w, m_hello_world_window.h), ImGuiCond_Always);
		}
		if (HelloWorldWindow.Begin("Hello, world!", &ShowHelloWorldWindow, HelloWorldWindow.Flags)) {
			ImVec2 pos = HelloWorldWindow.GetWindowPos();
			ImVec2 size = HelloWorldWindow.GetWindowSize();
			m_hello_world_window = { true, pos.x, pos.y, size.x, size.y };
			m_apply_hello_world_layout_once = false;

			ImGui::Text("This is some useful text.");
			ImGui::Checkbox("Demo Window", &ShowDemoWindow);
			ImGui::Checkbox("Another Window", &ShowAnotherWindow);
			if (style_editor)
				ImGui::Checkbox("Style Editor", &style_editor->IsOpen);
			ImGui::Checkbox("Debug Log", &ShowDebugLogMirrorWindow);
			ImGui::Checkbox("Terminals", &ShowTerminalWindow);
			ImGui::Checkbox("Test Engine", &ShowTestEngineWindow);
			ImGui::Checkbox("Emoji Atlas", &ShowEmojiAtlasWindow);
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::ColorEdit3("clear color", &ClearColor.x);
			if (ImGui::Button("Button"))
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
				1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			if (m_show_controls && *m_show_controls && m_draw_controls_section) {
				ImGui::SeparatorText("Controls");
				m_draw_controls_section();
			}
		}
		HelloWorldWindow.End();
	}

	// 3. Debug log mirror window.
	if (ShowDebugLogMirrorWindow)
		DebugLogMirror.ShowWindow(&ShowDebugLogMirrorWindow);

	// 4. Style editor window.
	if (style_editor)
		style_editor->Draw();

	// 5. Terminal window.
	if (ShowTerminalWindow)
		DrawTerminals();

	if (ShowEmojiAtlasWindow)
		DrawEmojiAtlasWindow();

	// 6. Show another simple window.
	if (ShowAnotherWindow) {
		if (m_apply_another_layout_once && m_another_window.valid) {
			AnotherWindow.SetNextPos(ImVec2(m_another_window.x, m_another_window.y), ImGuiCond_Always);
			AnotherWindow.SetNextSize(ImVec2(m_another_window.w, m_another_window.h), ImGuiCond_Always);
		}
		AnotherWindow.Begin("Another Window", &ShowAnotherWindow, AnotherWindow.Flags);
		{
			ImVec2 pos = AnotherWindow.GetWindowPos();
			ImVec2 size = AnotherWindow.GetWindowSize();
			m_another_window = { true, pos.x, pos.y, size.x, size.y };
			m_apply_another_layout_once = false;
		}
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			ShowAnotherWindow = false;
		AnotherWindow.End();
	}
}

void UiWindows::ApplyLayout(const WindowStateToml& state)
{
	m_hello_world_window = state.hello_world_window;
	m_terminals_window = state.terminals_window;
	m_emoji_atlas_window = state.emoji_atlas_window;
	m_another_window = state.another_window;
	m_apply_hello_world_layout_once = m_hello_world_window.valid;
	m_apply_terminals_layout_once = m_terminals_window.valid;
	m_apply_emoji_atlas_layout_once = m_emoji_atlas_window.valid;
	m_apply_another_layout_once = m_another_window.valid;
}

void UiWindows::ExportLayout(WindowStateToml* state) const
{
	if (!state)
		return;
	state->hello_world_window = m_hello_world_window;
	state->terminals_window = m_terminals_window;
	state->emoji_atlas_window = m_emoji_atlas_window;
	state->another_window = m_another_window;
}
