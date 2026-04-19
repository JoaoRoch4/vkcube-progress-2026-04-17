// Generated with ImRAD 0.10-WIP
// visit https://github.com/tpecholt/imrad

#pragma once
#include "imgui.h"
#include "app/scripting/cling_runtime.hpp"

#include <array>
#include <deque>
#include <string>

class ClingWindow {
    public:
	/// @begin interface
	void Open();
	void Close();
	void Draw();
	[[nodiscard]] bool IsOpen() const;
	/// @end interface

    private:
	/// @begin impl
	void DrawPopups();
	void RunSnippet();

	bool isOpen;
	std::array<char, 4096> m_input_buf;
	std::deque<std::string> m_output;
	ClingRuntime m_cling;
	bool m_cling_available;
	/// @end impl
};

extern ClingWindow clingWindow;
