#pragma once

#include <filesystem>

#include "app/hot_module.h"

// HotModule: dlopen-based hot-reload loader.
// Watches libhot.so on disk and reloads it whenever the file changes.
struct HotModule {
	using GetApiFn = HotModuleAPI* (*)();

	void* m_handle;
	HotModuleAPI* m_api;
	std::filesystem::file_time_type m_mtime;
	int m_gen;

	HotModule();

	// Resolve paths relative to the executable so the app works regardless
	// of the working directory it was launched from.
	static std::filesystem::path exe_dir();
	static std::filesystem::path src_path();

	void load();
	void tick();
	void build_ui(bool* p_open);
	void shutdown();
};
