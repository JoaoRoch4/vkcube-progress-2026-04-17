#pragma once

#include <string>
#include <string_view>

struct ClingRunResult {
	bool ok;
	int exit_code;
	std::string output;
};

// ClingRuntime executes C++ snippets via the Cling interactive C++ interpreter.
// When Cling is not installed, Execute() returns an error result with ok=false.
class ClingRuntime {
    public:
	ClingRuntime();

	// Returns true if the cling binary is reachable.
	[[nodiscard]] static bool IsAvailable();

	// Returns the path to the cling binary, or an empty string if not found.
	[[nodiscard]] static std::string FindExecutable();

	// Execute a C++ snippet (can span multiple lines).
	[[nodiscard]] ClingRunResult Execute(std::string_view snippet) const;

	// Evaluate a C++ expression and return its printed value.
	[[nodiscard]] ClingRunResult Probe(std::string_view expression) const;

    private:
	[[nodiscard]] ClingRunResult RunWithCling(std::string_view input) const;
};
