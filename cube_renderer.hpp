#pragma once

#include <sys/time.h>
#include <vulkan/vulkan.h>

// Renders the spinning cube into an already-open render pass.
// Usage:
//   cube.Init(device, phys_device, mem_props, render_pass, descriptor_pool);
//   // each frame, inside an open render pass:
//   cube.Draw(cmd, width, height);
//   cube.AddMouseDelta(dx, dy);   // call when dragging with right mouse button
//   cube.SetAnimate(false);       // pause auto-spin

class CubeRenderer {
public:
    CubeRenderer() = default;
    ~CubeRenderer();

    // Must be called once after VulkanContext::SetupWindow().
    // render_pass must be the pass that will be active when Draw() is called.
    void Init(VkDevice device,
              VkPhysicalDevice phys_device,
              VkPhysicalDeviceMemoryProperties mem_props,
              VkQueue queue,
              uint32_t queue_family,
              VkRenderPass render_pass,
              VkDescriptorPool descriptor_pool);

    void Cleanup();

    // Record cube draw calls into an already-open render pass command buffer.
    void Draw(VkCommandBuffer cmd, uint32_t width, uint32_t height);

    // Accumulate mouse drag delta for manual rotation (right-button drag).
    void AddMouseDelta(float dx, float dy);

    // Zoom in/out via scroll wheel (positive = closer, negative = farther).
    void AddScrollDelta(float delta);

    // Toggle automatic time-based rotation (default: on).
    void SetAnimate(bool on) { animate_ = on; }
    [[nodiscard]] bool GetAnimate() const { return animate_; }

    // Reset mouse-accumulated rotation to zero.
    void ResetMouseRotation();

private:
    [[nodiscard]] int FindHostCoherentMemory(uint32_t allowed) const;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties mem_props_{};

    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;

    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    void *map_ = nullptr;
    uint32_t vertex_offset_ = {};
    uint32_t colors_offset_ = {};
    uint32_t normals_offset_ = {};

    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE; // borrowed (not owned)
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

    struct timeval start_tv_{};
    float mouse_yaw_ = 0.0f;
    float mouse_pitch_ = 0.0f;
    float camera_distance_ = 8.0f; // distance along -Z
    bool animate_ = true;
    bool initialised_ = false;
};
