#pragma once

#include "app/renderer/vulkan/vulkan_context.hpp"

class CubeRenderer;

// VulkanContextWithCube extends VulkanContext to inject the spinning cube draw
// call inside the render pass, before the ImGui draw data submission.
class VulkanContextWithCube : public VulkanContext {
    public:
	VulkanContextWithCube();

	// Set the cube renderer and a pointer to the visibility flag.
	// Both must remain valid for the lifetime of this object.
	void SetCubeRenderer(CubeRenderer* renderer, bool* show_cube);

	// Injects cube draw + screen-capture readback before presenting.
	void FrameRender(ImDrawData* draw_data);

    private:
	CubeRenderer* m_cube_renderer;
	bool* m_show_cube;
};
