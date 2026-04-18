// Generated with ImRAD 0.10-WIP
// visit https://github.com/tpecholt/imrad

#pragma once
#include "imgui.h"

class SimpleWindow {
    public:
	SimpleWindow();

	/// @begin interface
	void Open();
	void Close();
	void Draw();
	[[nodiscard]] bool IsOpen() const;

	/// @end interface

    private:
	/// @begin impl
	void DrawPopups();

	bool isOpen;
	/// @end impl
};

extern SimpleWindow simpleWindow;
