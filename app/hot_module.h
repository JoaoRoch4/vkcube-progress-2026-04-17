#pragma once

#include <stdbool.h>

// Stable C ABI for the hot-reloadable module.
// The main executable calls these through function pointers so that
// libhot.so can be rebuilt and reloaded at runtime without restarting.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ImGuiContext ImGuiContext;

typedef struct
{
	// Called once after dlopen — set ImGui context and do any one-time setup.
	void (*init)(ImGuiContext* ctx);
	// Called every frame inside the ImGui NewFrame … EndFrame block.
	// p_open is forwarded to ImGui::Begin(..., p_open) so the host controls
	// window visibility and title-bar close behavior.
	void (*build_ui)(bool* p_open);
	// Called before dlclose — release any resources owned by the module.
	void (*shutdown)(void);
} HotModuleAPI;

// Every version of the hot module must export this symbol.
HotModuleAPI* hot_get_api(void);

#ifdef __cplusplus
}
#endif
