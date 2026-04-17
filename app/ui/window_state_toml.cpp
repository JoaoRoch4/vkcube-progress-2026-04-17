#include "app/ui/window_state_toml.hpp"

#include "rfl/toml.hpp"

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
	return result.has_value();
}
