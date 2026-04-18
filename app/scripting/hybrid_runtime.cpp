#include "hybrid_runtime.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <print>
#include <string>
#include <sys/wait.h>

namespace {

constexpr const char* kDefaultJsWorker = R"JS(import process from 'node:process';

function respond(ok, exitCode, message) {
  console.log(JSON.stringify({ ok, runtime: 'javascript', exitCode, message }));
  process.exit(exitCode);
}

const raw = process.argv.at(2) ?? '';
if (!raw) {
  respond(true, 0, 'js worker ready');
}

let req;
try {
  req = JSON.parse(raw);
} catch (err) {
  respond(false, 2, `invalid json request: ${String(err)}`);
}

const command = typeof req.command === 'string' ? req.command : 'execute';
const prompt = typeof req.prompt === 'string' ? req.prompt : '';

if (command === 'ping') {
  respond(true, 0, 'pong from javascript worker');
}
if (command === 'echo') {
  respond(true, 0, prompt);
}
if (command === 'execute') {
  respond(true, 0, `js processed: ${prompt}`);
}

respond(false, 3, `unknown command: ${command}`);
)JS";

constexpr const char* kDefaultPythonWorker = R"PY(import json
import sys


def respond(ok: bool, exit_code: int, message: str) -> None:
    print(json.dumps({"ok": ok, "runtime": "python", "exitCode": exit_code, "message": message}))
    raise SystemExit(exit_code)


raw = sys.argv[1] if len(sys.argv) > 1 else ""
if not raw:
    respond(True, 0, "python worker ready")

try:
    req = json.loads(raw)
except Exception as exc:
    respond(False, 2, f"invalid json request: {exc}")

command = req.get("command", "execute") if isinstance(req, dict) else "execute"
prompt = req.get("prompt", "") if isinstance(req, dict) else ""

if command == "ping":
    respond(True, 0, "pong from python worker")
if command == "echo":
    respond(True, 0, str(prompt))
if command == "execute":
    respond(True, 0, f"python processed: {prompt}")

respond(False, 3, f"unknown command: {command}")
)PY";

std::string Trim(std::string value)
{
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		value.pop_back();
	std::size_t i = 0;
	while (i < value.size() && std::isspace(static_cast<unsigned char>(value.at(i))))
		++i;
	if (i > 0)
		value.erase(0, i);
	return value;
}

std::string JsonEscape(std::string_view value)
{
	std::string escaped {};
	escaped.reserve(value.size() + 8);
	for (char c : value) {
		switch (c) {
		case '\\':
			escaped += "\\\\";
			break;
		case '"':
			escaped += "\\\"";
			break;
		case '\n':
			escaped += "\\n";
			break;
		case '\r':
			escaped += "\\r";
			break;
		case '\t':
			escaped += "\\t";
			break;
		default:
			escaped.push_back(c);
			break;
		}
	}
	return escaped;
}

std::string BuildRequestJson(std::string_view prompt)
{
	std::string command { "execute" };
	std::string payload { prompt };
	if (prompt == "ping") {
		command = "ping";
		payload.clear();
	} else if (prompt.size() >= 5 && prompt.substr(0, 5) == "echo ") {
		command = "echo";
		payload = std::string { prompt.substr(5) };
	}

	return std::string { "{\"command\":\"" } + JsonEscape(command)
		+ "\",\"prompt\":\"" + JsonEscape(payload) + "\"}";
}

bool JsonBoolField(std::string_view json, std::string_view key, bool* out)
{
	if (!out)
		return false;
	const std::string needle = std::string { "\"" } + std::string { key } + "\":";
	const std::size_t pos = json.find(needle);
	if (pos == std::string_view::npos)
		return false;
	const std::size_t value_pos = pos + needle.size();
	if (json.size() >= value_pos + 4 && json.substr(value_pos, 4) == "true") {
		*out = true;
		return true;
	}
	if (json.size() >= value_pos + 5 && json.substr(value_pos, 5) == "false") {
		*out = false;
		return true;
	}
	return false;
}

bool JsonIntField(std::string_view json, std::string_view key, int* out)
{
	if (!out)
		return false;
	const std::string needle = std::string { "\"" } + std::string { key } + "\":";
	const std::size_t pos = json.find(needle);
	if (pos == std::string_view::npos)
		return false;
	std::size_t i = pos + needle.size();
	if (i >= json.size())
		return false;
	bool neg = false;
	if (json.at(i) == '-') {
		neg = true;
		++i;
	}
	if (i >= json.size() || !std::isdigit(static_cast<unsigned char>(json.at(i))))
		return false;
	int value = 0;
	while (i < json.size() && std::isdigit(static_cast<unsigned char>(json.at(i)))) {
		value = (value * 10) + (json.at(i) - '0');
		++i;
	}
	*out = neg ? -value : value;
	return true;
}

bool JsonStringField(std::string_view json, std::string_view key, std::string* out)
{
	if (!out)
		return false;
	const std::string needle = std::string { "\"" } + std::string { key } + "\":\"";
	const std::size_t pos = json.find(needle);
	if (pos == std::string_view::npos)
		return false;
	std::size_t i = pos + needle.size();
	std::string decoded {};
	while (i < json.size()) {
		const char c = json.at(i);
		if (c == '"') {
			*out = decoded;
			return true;
		}
		if (c == '\\' && i + 1 < json.size()) {
			const char esc = json.at(i + 1);
			switch (esc) {
			case 'n':
				decoded.push_back('\n');
				break;
			case 'r':
				decoded.push_back('\r');
				break;
			case 't':
				decoded.push_back('\t');
				break;
			case '\\':
				decoded.push_back('\\');
				break;
			case '"':
				decoded.push_back('"');
				break;
			default:
				decoded.push_back(esc);
				break;
			}
			i += 2;
			continue;
		}
		decoded.push_back(c);
		++i;
	}
	return false;
}

bool WriteTextFile(const std::filesystem::path& path, std::string_view content)
{
	std::ofstream out { path };
	if (!out.is_open())
		return false;
	out << content;
	return out.good();
}

std::string ResolvePythonExecutable(const std::filesystem::path& project_root)
{
	if (const char* configured = std::getenv("HYBRID_PYTHON")) {
		if (configured[0] != '\0')
			return std::string { configured };
	}

	if (const char* conda_prefix = std::getenv("CONDA_PREFIX")) {
		if (conda_prefix[0] != '\0') {
			const std::filesystem::path candidate = std::filesystem::path { conda_prefix } / "bin" / "python";
			if (std::filesystem::exists(candidate))
				return candidate.string();
		}
	}

	const std::filesystem::path local_env = project_root / ".conda" / "hybrid" / "bin" / "python";
	if (std::filesystem::exists(local_env))
		return local_env.string();

	return "python3";
}

} // namespace

HybridScriptRuntime::HybridScriptRuntime()
    : m_project_root {}
    , m_hybrid_dir {}
    , m_js_worker_path {}
    , m_python_worker_path {}
{
}

void HybridScriptRuntime::Init(const std::filesystem::path& project_root)
{
	m_project_root = project_root;
	m_hybrid_dir = m_project_root / "scripts" / "hybrid";
	m_js_worker_path = m_hybrid_dir / "js_worker.mjs";
	m_python_worker_path = m_hybrid_dir / "py_worker.py";
	EnsureDefaultScripts();
}

HybridRunResult HybridScriptRuntime::RunJavaScript(std::string_view prompt) const
{
	return RunWithCommand("javascript", m_js_worker_path, "node", prompt);
}

HybridRunResult HybridScriptRuntime::RunPython(std::string_view prompt) const
{
	const std::string python_executable = ResolvePythonExecutable(m_project_root);
	return RunWithCommand("python", m_python_worker_path, python_executable.c_str(), prompt);
}

HybridRunResult HybridScriptRuntime::RunWithCommand(const char* runtime_name,
	const std::filesystem::path& script_path,
	const char* executable,
	std::string_view prompt) const
{
	if (script_path.empty()) {
		return { false, runtime_name, -1, "runtime is not initialized" };
	}

	if (!std::filesystem::exists(script_path)) {
		return { false, runtime_name, -1,
			std::string { "worker script missing: " } + script_path.string() };
	}

	const std::string request_json = BuildRequestJson(prompt);
	const std::string cmd = std::string { executable } + " " + ShellQuote(script_path.string()) + " "
		+ ShellQuote(request_json) + " 2>&1";
	FILE* pipe = ::popen(cmd.c_str(), "r");
	if (!pipe) {
		return { false, runtime_name, -1, "failed to spawn worker process" };
	}

	std::array<char, 512> buffer {};
	std::string output {};
	while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
		output += buffer.data();
	}

	const int raw_status = ::pclose(pipe);
	const int exit_code = DecodeExitCode(raw_status);
	output = Trim(output);
	if (output.empty())
		output = "(no output)";

	bool ok = (exit_code == 0);
	int worker_exit_code = exit_code;
	std::string worker_runtime { runtime_name };
	std::string worker_message { output };
	bool worker_ok = ok;
	if (JsonBoolField(output, "ok", &worker_ok))
		ok = worker_ok && (exit_code == 0);
	if (JsonIntField(output, "exitCode", &worker_exit_code))
		ok = ok && (worker_exit_code == 0);
	if (JsonStringField(output, "runtime", &worker_runtime))
		ok = ok && !worker_runtime.empty();
	if (JsonStringField(output, "message", &worker_message))
		return { ok, worker_runtime, worker_exit_code, worker_message };

	return { ok, worker_runtime, worker_exit_code, output };
}

std::string HybridScriptRuntime::ShellQuote(std::string_view value)
{
	std::string quoted {};
	quoted.reserve(value.size() + 8);
	quoted += '\'';
	for (char c : value) {
		if (c == '\'') {
			quoted += "'\\''";
		} else {
			quoted.push_back(c);
		}
	}
	quoted += '\'';
	return quoted;
}

int HybridScriptRuntime::DecodeExitCode(int status)
{
	if (status == -1)
		return -1;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return status;
}

void HybridScriptRuntime::EnsureDefaultScripts() const
{
	if (m_hybrid_dir.empty())
		return;

	std::error_code ec {};
	std::filesystem::create_directories(m_hybrid_dir, ec);
	if (ec) {
		std::println(stderr, "[hybrid] could not create scripts directory: {}", ec.message());
		return;
	}

	if (!std::filesystem::exists(m_js_worker_path)) {
		if (!WriteTextFile(m_js_worker_path, kDefaultJsWorker))
			std::println(stderr, "[hybrid] failed to write {}", m_js_worker_path.string());
	}
	if (!std::filesystem::exists(m_python_worker_path)) {
		if (!WriteTextFile(m_python_worker_path, kDefaultPythonWorker))
			std::println(stderr, "[hybrid] failed to write {}", m_python_worker_path.string());
	}
}
