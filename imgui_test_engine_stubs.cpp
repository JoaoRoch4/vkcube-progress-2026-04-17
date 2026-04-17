// Stub implementations for imgui_test_engine hooks.
// These are needed when IMGUI_ENABLE_TEST_ENGINE is defined in imconfig.h
// but we are not linking against the test engine library.

#include "imgui.h"
#include "imgui_internal.h"

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID, const ImRect&, const ImGuiLastItemData*) {}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID, const char*, ImGuiItemStatusFlags) {}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }
