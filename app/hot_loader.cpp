#include "app/hot_loader.hpp"

#include <bit>
#include <format>
#include <print>

#include <dlfcn.h>

#include "imgui.h"

HotModule::HotModule()
    : m_handle { nullptr }
    , m_api { nullptr }
    , m_mtime {}
    , m_gen { 0 }
{
}

std::filesystem::path HotModule::exe_dir()
{
	std::error_code ec;
	auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
	return ec ? std::filesystem::current_path() : p.parent_path();
}

std::filesystem::path HotModule::src_path()
{
	return exe_dir() / "libhot.so";
}

void HotModule::load()
{
	const auto src = src_path();

	// Copy to a uniquely-named file: Linux caches dlopen by inode,
	// so re-opening the same path returns the old handle.
	m_gen = (m_gen + 1) % 8;
	auto tmp = exe_dir() / std::format("libhot_{}.so", m_gen);

	std::error_code ec;
	std::filesystem::copy_file(src, tmp,
		std::filesystem::copy_options::overwrite_existing, ec);
	if (ec) {
		std::println(stderr, "[hot] copy failed: {}", ec.message());
		return;
	}

	// Unload previous generation.
	if (m_api && m_api->shutdown)
		m_api->shutdown();
	if (m_handle) {
		dlclose(m_handle);
		m_handle = nullptr;
		m_api = nullptr;
	}

	m_handle = dlopen(tmp.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!m_handle) {
		std::println(stderr, "[hot] dlopen: {}", dlerror());
		return;
	}

	auto fn = std::bit_cast<GetApiFn>(dlsym(m_handle, "hot_get_api"));
	if (!fn) {
		std::println(stderr, "[hot] dlsym: {}", dlerror());
		dlclose(m_handle);
		m_handle = nullptr;
		return;
	}

	m_api = fn();
	if (m_api && m_api->init)
		m_api->init(ImGui::GetCurrentContext());

	m_mtime = std::filesystem::last_write_time(src, ec);
	std::println("[hot] reloaded (gen {})", m_gen);
}

void HotModule::tick()
{
	std::error_code ec;
	auto t = std::filesystem::last_write_time(src_path(), ec);
	if (!ec && t != m_mtime)
		load();
}

void HotModule::build_ui(bool* p_open)
{
	if (m_api && m_api->build_ui)
		m_api->build_ui(p_open);
}

void HotModule::shutdown()
{
	if (m_api && m_api->shutdown)
		m_api->shutdown();
	if (m_handle) {
		dlclose(m_handle);
		m_handle = nullptr;
	}
	m_api = nullptr;
}
