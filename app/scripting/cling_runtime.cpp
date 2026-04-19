#include "cling_runtime.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// Candidate binary names / absolute paths for the cling interpreter.
constexpr std::array kClingCandidates {
	"/usr/bin/cling",
	"/usr/local/bin/cling",
	"/opt/cling/bin/cling",
};

std::string FindClingBinary()
{
	if (const char* env = std::getenv("CLING_BIN"); env && env[0] != '\0') {
		if (std::filesystem::exists(env))
			return std::string { env };
	}
	for (const char* path : kClingCandidates) {
		if (std::filesystem::exists(path))
			return std::string { path };
	}
	// Try PATH via `which`
	FILE* pipe = ::popen("command -v cling 2>/dev/null", "r");
	if (pipe) {
		std::array<char, 256> buf {};
		std::string result {};
		while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
			result += buf.data();
		::pclose(pipe);
		// Strip trailing newline
		while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
			result.pop_back();
		if (!result.empty() && std::filesystem::exists(result))
			return result;
	}
	return {};
}

int DecodeExitCode(int status)
{
	if (status == -1)
		return -1;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return status;
}

std::string Trim(std::string value)
{
	while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' '))
		value.pop_back();
	std::size_t i = 0;
	while (i < value.size() && (value.at(i) == '\n' || value.at(i) == '\r' || value.at(i) == ' '))
		++i;
	if (i > 0)
		value.erase(0, i);
	return value;
}

} // namespace

ClingRuntime::ClingRuntime() = default;

bool ClingRuntime::IsAvailable()
{
	return !FindClingBinary().empty();
}

std::string ClingRuntime::FindExecutable()
{
	return FindClingBinary();
}

ClingRunResult ClingRuntime::Execute(std::string_view snippet) const
{
	// Append a cling quit command so the interpreter does not hang waiting for
	// more input after processing the snippet.
	std::string input { snippet };
	if (!input.empty() && input.back() != '\n')
		input += '\n';
	input += ".q\n";
	return RunWithCling(input);
}

ClingRunResult ClingRuntime::Probe(std::string_view expression) const
{
	// Wrap the expression in a print statement so the value is visible.
	std::string input { "#include <iostream>\n" };
	input += "std::cout << (";
	input += expression;
	input += ") << std::endl;\n.q\n";
	return RunWithCling(input);
}

ClingRunResult ClingRuntime::RunWithCling(std::string_view input) const
{
	const std::string binary = FindClingBinary();
	if (binary.empty()) {
		return { false, -1,
			"cling interpreter not found. "
			"Install cling or set CLING_BIN=/path/to/cling." };
	}

	// Write input to a temporary file and redirect it to cling's stdin.
	const std::filesystem::path tmp_path = std::filesystem::temp_directory_path() / "vkcube_cling_snippet.cpp";
	{
		std::ofstream tmp { tmp_path };
		if (!tmp.is_open()) {
			return { false, -1, "failed to create temporary cling input file" };
		}
		tmp << input;
	}

	const std::string cmd = binary + " --nologo < " + tmp_path.string() + " 2>&1";
	FILE* pipe = ::popen(cmd.c_str(), "r");
	if (!pipe) {
		std::filesystem::remove(tmp_path);
		return { false, -1, "failed to spawn cling process" };
	}

	std::array<char, 512> buffer {};
	std::string output {};
	while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
		output += buffer.data();

	const int raw_status = ::pclose(pipe);
	std::filesystem::remove(tmp_path);

	const int exit_code = DecodeExitCode(raw_status);
	output = Trim(output);
	if (output.empty())
		output = "(no output)";

	// Strip the cling banner/prompt lines that clutter the output.
	// Cling emits prompts like "[cling]$ " and "[cling]! " on each line.
	std::string clean {};
	clean.reserve(output.size());
	std::size_t i = 0;
	while (i < output.size()) {
		const std::size_t nl = output.find('\n', i);
		const std::string_view line { output.data() + i,
			(nl == std::string::npos ? output.size() : nl) - i };
		const bool is_prompt = (line.rfind("[cling]", 0) == 0);
		if (!is_prompt) {
			clean += line;
			clean += '\n';
		}
		i = (nl == std::string::npos ? output.size() : nl + 1);
	}
	while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r'))
		clean.pop_back();
	if (clean.empty())
		clean = output; // fallback: keep raw output

	return { exit_code == 0, exit_code, clean };
}
