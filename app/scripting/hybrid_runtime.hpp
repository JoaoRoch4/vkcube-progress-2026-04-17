#pragma once

#include <filesystem>
#include <string>
#include <string_view>

struct HybridRunResult {
	bool ok;
	std::string runtime;
	int exit_code;
	std::string output;
};

class HybridScriptRuntime {
    public:
	HybridScriptRuntime();

	void Init(const std::filesystem::path& project_root);
	[[nodiscard]] HybridRunResult RunJavaScript(std::string_view prompt) const;
	[[nodiscard]] HybridRunResult RunPython(std::string_view prompt) const;

    private:
	[[nodiscard]] HybridRunResult RunWithCommand(const char* runtime_name,
		const std::filesystem::path& script_path,
		const char* executable,
		std::string_view prompt) const;
	[[nodiscard]] static std::string ShellQuote(std::string_view value);
	[[nodiscard]] static int DecodeExitCode(int status);
	void EnsureDefaultScripts() const;

	std::filesystem::path m_project_root;
	std::filesystem::path m_hybrid_dir;
	std::filesystem::path m_js_worker_path;
	std::filesystem::path m_python_worker_path;
};
