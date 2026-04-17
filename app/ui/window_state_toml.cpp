#include "window_state_toml.hpp"

#include <rfl/toml.hpp>

#include <array>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct CommentRule {
	const char* prefix;
	const char* comment;
};

std::string LTrim(const std::string& text)
{
	size_t index = 0;
	while (index < text.size() && std::isspace(static_cast<unsigned char>(text.at(index))))
		++index;
	return text.substr(index);
}

std::string Trim(const std::string& text)
{
	if (text.empty())
		return text;
	size_t start = 0;
	while (start < text.size() && std::isspace(static_cast<unsigned char>(text.at(start))))
		++start;
	if (start == text.size())
		return {};
	size_t end = text.size();
	while (end > start && std::isspace(static_cast<unsigned char>(text.at(end - 1))))
		--end;
	return text.substr(start, end - start);
}

bool IsHeaderLine(const std::string& trimmed)
{
	return (trimmed.starts_with("[") && trimmed.ends_with("]"));
}

std::string HeaderName(const std::string& trimmed)
{
	if (!IsHeaderLine(trimmed))
		return {};
	if (trimmed.starts_with("[[") && trimmed.ends_with("]]"))
		return trimmed.substr(2, trimmed.size() - 4);
	return trimmed.substr(1, trimmed.size() - 2);
}

bool IsKeyValueLine(const std::string& trimmed)
{
	if (trimmed.empty() || trimmed.starts_with("#"))
		return false;
	return trimmed.find('=') != std::string::npos;
}

std::string KeyName(const std::string& trimmed)
{
	const size_t equals_pos = trimmed.find('=');
	if (equals_pos == std::string::npos)
		return {};
	return Trim(trimmed.substr(0, equals_pos));
}

std::string FieldCommentFor(const std::string& section_name, const std::string& key,
	int active_style_color_index, const std::array<const char*, ImGuiCol_COUNT>& style_color_names)
{
	if (key == "show_controls")
		return "# Show embedded Controls section inside Hello, world! window.";
	if (key == "show_copilot")
		return "# Show Copilot chat/testing window.";
	if (key == "show_hot_module")
		return "# Show hot-reload module UI window.";
	if (key == "show_cube")
		return "# Show cube in Vulkan background.";
	if (key == "cube_auto_spin")
		return "# Auto-rotate cube each frame when true.";
	if (key == "show_style_editor_window")
		return "# Show Style Editor window.";
	if (key == "show_demo_window")
		return "# Show Dear ImGui demo window.";
	if (key == "show_another_window")
		return "# Show the 'Another Window' sample panel.";
	if (key == "show_debug_log_mirror_window")
		return "# Show debug log mirror window.";
	if (key == "show_terminal_window")
		return "# Show terminals window.";
	if (key == "show_test_engine_window")
		return "# Show imgui_test_engine control window.";
	if (key == "show_emoji_atlas_window")
		return "# Show emoji atlas preview window.";

	if (section_name == "clear_color") {
		if (key == "x")
			return "# Red channel (0..1).";
		if (key == "y")
			return "# Green channel (0..1).";
		if (key == "z")
			return "# Blue channel (0..1).";
		if (key == "w")
			return "# Alpha channel (0..1).";
	}

	const bool is_window_rect_section = section_name.ends_with("_window") || section_name == "controls_window";
	if (is_window_rect_section) {
		if (key == "x")
			return "# Window left position in pixels.";
		if (key == "y")
			return "# Window top position in pixels.";
		if (key == "w")
			return "# Window width in pixels.";
		if (key == "h")
			return "# Window height in pixels.";
		if (key == "valid")
			return "# True if this saved rectangle should be applied.";
	}

	if (section_name == "style.colors") {
		std::string color_name = "ImGuiCol[" + std::to_string(active_style_color_index) + "]";
		if (active_style_color_index >= 0
			&& active_style_color_index < static_cast<int>(style_color_names.size())) {
			color_name = style_color_names.at(static_cast<size_t>(active_style_color_index));
		}
		if (key == "x")
			return "# " + color_name + " red channel.";
		if (key == "y")
			return "# " + color_name + " green channel.";
		if (key == "z")
			return "# " + color_name + " blue channel.";
		if (key == "w")
			return "# " + color_name + " alpha channel.";
	}

	if (key == "x")
		return "# X component.";
	if (key == "y")
		return "# Y component.";
	if (key == "z")
		return "# Z component.";
	if (key == "w")
		return "# W component.";

	if (section_name.starts_with("style"))
		return "# Saved Dear ImGui style value for '" + key + "'.";
	return "# Saved value for '" + key + "'.";
}

void AnnotateWindowStateToml(const std::filesystem::path& file_path)
{
	std::ifstream input(file_path);
	if (!input.is_open())
		return;

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(input, line))
		lines.push_back(line);
	input.close();

	const std::array<CommentRule, 12> rules {
		CommentRule { "show_controls =", "# Window toggles persisted between runs." },
		CommentRule { "show_cube =", "# Cube background visibility (false uses clear_color as scene background)." },
		CommentRule { "cube_auto_spin =", "# Cube animation toggle (true = auto rotate, false = manual drag only)." },
		CommentRule { "[clear_color]", "# Clear color used when cube background is disabled." },
		CommentRule { "[controls_window]", "# Legacy layout section retained for compatibility." },
		CommentRule { "[copilot_window]", "# Window rectangle fields: x/y = top-left, w/h = size, valid = apply saved rect." },
		CommentRule { "[hello_world_window]", "# Hello, world! window rectangle." },
		CommentRule { "[style]", "# Full Dear ImGui style snapshot (preset + all style fields)." },
		CommentRule { "[style_editor_window]", "# Style Editor window rectangle." },
		CommentRule { "[terminals_window]", "# Terminals window rectangle." },
		CommentRule { "    [style.window_padding]", "# Vec2 values: x = horizontal, y = vertical." },
		CommentRule { "    [[style.colors]]", "# style.colors is an ordered array: entry N maps to ImGuiCol enum value N; x=r, y=g, z=b, w=a." },
	};
	const std::array<const char*, ImGuiCol_COUNT> style_color_names {
		"ImGuiCol_Text",
		"ImGuiCol_TextDisabled",
		"ImGuiCol_WindowBg",
		"ImGuiCol_ChildBg",
		"ImGuiCol_PopupBg",
		"ImGuiCol_Border",
		"ImGuiCol_BorderShadow",
		"ImGuiCol_FrameBg",
		"ImGuiCol_FrameBgHovered",
		"ImGuiCol_FrameBgActive",
		"ImGuiCol_TitleBg",
		"ImGuiCol_TitleBgActive",
		"ImGuiCol_TitleBgCollapsed",
		"ImGuiCol_MenuBarBg",
		"ImGuiCol_ScrollbarBg",
		"ImGuiCol_ScrollbarGrab",
		"ImGuiCol_ScrollbarGrabHovered",
		"ImGuiCol_ScrollbarGrabActive",
		"ImGuiCol_CheckMark",
		"ImGuiCol_SliderGrab",
		"ImGuiCol_SliderGrabActive",
		"ImGuiCol_Button",
		"ImGuiCol_ButtonHovered",
		"ImGuiCol_ButtonActive",
		"ImGuiCol_Header",
		"ImGuiCol_HeaderHovered",
		"ImGuiCol_HeaderActive",
		"ImGuiCol_Separator",
		"ImGuiCol_SeparatorHovered",
		"ImGuiCol_SeparatorActive",
		"ImGuiCol_ResizeGrip",
		"ImGuiCol_ResizeGripHovered",
		"ImGuiCol_ResizeGripActive",
		"ImGuiCol_InputTextCursor",
		"ImGuiCol_TabHovered",
		"ImGuiCol_Tab",
		"ImGuiCol_TabSelected",
		"ImGuiCol_TabSelectedOverline",
		"ImGuiCol_TabDimmed",
		"ImGuiCol_TabDimmedSelected",
		"ImGuiCol_TabDimmedSelectedOverline",
		"ImGuiCol_PlotLines",
		"ImGuiCol_PlotLinesHovered",
		"ImGuiCol_PlotHistogram",
		"ImGuiCol_PlotHistogramHovered",
		"ImGuiCol_TableHeaderBg",
		"ImGuiCol_TableBorderStrong",
		"ImGuiCol_TableBorderLight",
		"ImGuiCol_TableRowBg",
		"ImGuiCol_TableRowBgAlt",
		"ImGuiCol_TextLink",
		"ImGuiCol_TextSelectedBg",
		"ImGuiCol_TreeLines",
		"ImGuiCol_DragDropTarget",
		"ImGuiCol_DragDropTargetBg",
		"ImGuiCol_UnsavedMarker",
		"ImGuiCol_NavCursor",
		"ImGuiCol_NavWindowingHighlight",
		"ImGuiCol_NavWindowingDimBg",
		"ImGuiCol_ModalWindowDimBg",
	};
	std::array<bool, rules.size()> emitted_rule_comment {};
	size_t style_color_index = 0;
	int active_style_color_index = -1;
	std::string current_section;

	std::vector<std::string> annotated;
	annotated.reserve(lines.size() * 2 + 32);
	annotated.push_back("# vkcube persisted UI state (auto-generated). Manual edits are allowed.");
	for (const std::string& current : lines) {
		const std::string trimmed = LTrim(current);
		if (IsHeaderLine(trimmed)) {
			current_section = HeaderName(trimmed);
			if (current_section != "style.colors")
				active_style_color_index = -1;
		}

		if (trimmed.starts_with("[[style.colors]]")) {
			active_style_color_index = static_cast<int>(style_color_index);
			if (style_color_index < style_color_names.size()) {
				annotated.push_back("# style.colors[" + std::to_string(style_color_index)
					+ "] = " + style_color_names.at(style_color_index));
			}
			++style_color_index;
		}
		for (size_t rule_index = 0; rule_index < rules.size(); ++rule_index) {
			const CommentRule& rule = rules.at(rule_index);
			if (trimmed.starts_with(rule.prefix)) {
				if (!emitted_rule_comment.at(rule_index)) {
					annotated.push_back(rule.comment);
					emitted_rule_comment.at(rule_index) = true;
				}
				break;
			}
		}

		if (IsKeyValueLine(trimmed)) {
			const std::string key = KeyName(trimmed);
			const std::string field_comment = FieldCommentFor(current_section, key,
				active_style_color_index, style_color_names);
			if (!field_comment.empty())
				annotated.push_back(field_comment);
		}
		annotated.push_back(current);
	}

	std::ofstream output(file_path, std::ios::trunc);
	if (!output.is_open())
		return;
	for (const std::string& annotated_line : annotated)
		output << annotated_line << '\n';
}

} // namespace

bool LoadWindowStateToml(const std::filesystem::path& file_path, WindowStateToml& state)
{
	const auto result = rfl::toml::load<WindowStateToml>(file_path.string());
	if (!result)
		return false;
	state = result.value();
	return true;
}

bool SaveWindowStateToml(const std::filesystem::path& file_path, const WindowStateToml& state)
{
	const auto result = rfl::toml::save(file_path.string(), state);
	if (!result.has_value())
		return false;
	AnnotateWindowStateToml(file_path);
	return true;
}
