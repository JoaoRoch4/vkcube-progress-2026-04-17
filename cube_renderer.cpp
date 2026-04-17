#include "cube_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <print>
#include <sys/time.h>

extern "C" {
#include "esUtil.h"
}

// SPIR-V headers are generated into ${SPIRV_OUTPUT_DIR}/ by CMake.
static uint32_t vs_spirv[] = {
#include "vkcube.vert.spv.h"
};
static uint32_t fs_spirv[] = {
#include "vkcube.frag.spv.h"
};

struct Ubo {
    ESMatrix modelview;
    ESMatrix modelviewprojection;
    float normal[12]; // mat3 laid out as 3 vec4s
};

// ── Geometry data ─────────────────────────────────────────────────────────────

static constexpr std::array<float, 72> kVertices = {{
    // front
    -1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    -1.0f,
    +1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    // back
    +1.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    -1.0f,
    -1.0f,
    +1.0f,
    -1.0f,
    // right
    +1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    -1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    -1.0f,
    // left
    -1.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    +1.0f,
    -1.0f,
    +1.0f,
    -1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    // top
    -1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    +1.0f,
    -1.0f,
    +1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    -1.0f,
    // bottom
    -1.0f,
    -1.0f,
    -1.0f,
    +1.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    +1.0f,
    +1.0f,
    -1.0f,
    +1.0f,
}};

static constexpr std::array<float, 72> kColors = {{
    // front
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    0.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    // back
    1.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    // right
    1.0f,
    0.0f,
    1.0f,
    1.0f,
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    // left
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    // top
    0.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    0.0f,
    1.0f,
    1.0f,
    0.0f,
    // bottom
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
}};

static constexpr std::array<float, 72> kNormals = {{
    // front  (×4)
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    // back   (×4)
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    // right  (×4)
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    // left   (×4)
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    // top    (×4)
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    0.0f,
    +1.0f,
    0.0f,
    // bottom (×4)
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    0.0f,
}};

// ─────────────────────────────────────────────────────────────────────────────

int CubeRenderer::FindHostCoherentMemory(uint32_t allowed) const {
    for (uint32_t i = 0; (1u << i) <= allowed && i < mem_props_.memoryTypeCount; ++i) {
        auto flags = mem_props_.memoryTypes[i].propertyFlags;
        if ((allowed & (1u << i)) &&
            (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            return static_cast<int>(i);
    }
    return -1;
}

static void check_vk(VkResult r, const char *msg) {
    if (r != VK_SUCCESS) {
        std::println(stderr, "Vulkan error {}: {}", static_cast<int>(r), msg);
        std::abort();
    }
}

void CubeRenderer::Init(VkDevice device,
                        VkPhysicalDevice /*phys_device*/,
                        VkPhysicalDeviceMemoryProperties mem_props,
                        VkQueue /*queue*/,
                        uint32_t /*queue_family*/,
                        VkRenderPass render_pass,
                        VkDescriptorPool descriptor_pool) {
    device_ = device;
    mem_props_ = mem_props;
    desc_pool_ = descriptor_pool;
    gettimeofday(&start_tv_, nullptr);

    // ── Descriptor set layout ─────────────────────────────────────────────
    const VkDescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };
    const VkDescriptorSetLayoutCreateInfo dsl_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    check_vk(vkCreateDescriptorSetLayout(device_, &dsl_info, nullptr, &set_layout_),
             "vkCreateDescriptorSetLayout");

    // ── Pipeline layout ───────────────────────────────────────────────────
    const VkPipelineLayoutCreateInfo pl_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout_,
    };
    check_vk(vkCreatePipelineLayout(device_, &pl_info, nullptr, &pipeline_layout_),
             "vkCreatePipelineLayout");

    // ── Shader modules ────────────────────────────────────────────────────
    VkShaderModule vs_module{};
    const VkShaderModuleCreateInfo vs_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(vs_spirv),
        .pCode = vs_spirv,
    };
    check_vk(vkCreateShaderModule(device_, &vs_info, nullptr, &vs_module),
             "vkCreateShaderModule (vert)");

    VkShaderModule fs_module{};
    const VkShaderModuleCreateInfo fs_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(fs_spirv),
        .pCode = fs_spirv,
    };
    check_vk(vkCreateShaderModule(device_, &fs_info, nullptr, &fs_module),
             "vkCreateShaderModule (frag)");

    // ── Graphics pipeline ─────────────────────────────────────────────────
    const std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vs_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fs_module,
            .pName = "main",
        },
    }};

    const std::array<VkVertexInputBindingDescription, 3> bindings{{
        {0, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX},
        {1, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX},
        {2, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX},
    }};
    const std::array<VkVertexInputAttributeDescription, 3> attribs{{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {2, 2, VK_FORMAT_R32G32B32_SFLOAT, 0},
    }};
    const VkPipelineVertexInputStateCreateInfo vi{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size()),
        .pVertexBindingDescriptions = bindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size()),
        .pVertexAttributeDescriptions = attribs.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo ia{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    const VkPipelineViewportStateCreateInfo vp{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const VkPipelineRasterizationStateCreateInfo raster{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    const VkPipelineMultisampleStateCreateInfo ms{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    const VkPipelineDepthStencilStateCreateInfo ds{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    const VkPipelineColorBlendAttachmentState blend_att{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo blend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_att,
    };
    const std::array<VkDynamicState, 2> dyn_states{{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    }};
    const VkPipelineDynamicStateCreateInfo dyn{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dyn_states.size()),
        .pDynamicStates = dyn_states.data(),
    };
    const VkGraphicsPipelineCreateInfo pipe_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vi,
        .pInputAssemblyState = &ia,
        .pViewportState = &vp,
        .pRasterizationState = &raster,
        .pMultisampleState = &ms,
        .pDepthStencilState = &ds,
        .pColorBlendState = &blend,
        .pDynamicState = &dyn,
        .layout = pipeline_layout_,
        .renderPass = render_pass,
        .subpass = 0,
    };
    check_vk(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &pipeline_),
             "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(device_, vs_module, nullptr);
    vkDestroyShaderModule(device_, fs_module, nullptr);

    // ── GPU buffer (UBO + vertices + colors + normals) ────────────────────
    vertex_offset_ = sizeof(Ubo);
    colors_offset_ = vertex_offset_ + static_cast<uint32_t>(kVertices.size() * sizeof(float));
    normals_offset_ = colors_offset_ + static_cast<uint32_t>(kColors.size() * sizeof(float));
    const uint32_t mem_size = normals_offset_ + static_cast<uint32_t>(kNormals.size() * sizeof(float));

    const VkBufferCreateInfo buf_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = mem_size,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    check_vk(vkCreateBuffer(device_, &buf_info, nullptr, &buffer_), "vkCreateBuffer");

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(device_, buffer_, &reqs);

    const int mt = FindHostCoherentMemory(reqs.memoryTypeBits);
    if (mt < 0) {
        std::println(stderr, "CubeRenderer: no host-coherent memory type");
        std::abort();
    }
    const VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_size,
        .memoryTypeIndex = static_cast<uint32_t>(mt),
    };
    check_vk(vkAllocateMemory(device_, &alloc_info, nullptr, &memory_), "vkAllocateMemory");
    check_vk(vkMapMemory(device_, memory_, 0, mem_size, 0, &map_), "vkMapMemory");
    check_vk(vkBindBufferMemory(device_, buffer_, memory_, 0), "vkBindBufferMemory");

    // Upload static geometry
    auto *base = static_cast<uint8_t *>(map_);
    std::memcpy(base + vertex_offset_, kVertices.data(), kVertices.size() * sizeof(float));
    std::memcpy(base + colors_offset_, kColors.data(), kColors.size() * sizeof(float));
    std::memcpy(base + normals_offset_, kNormals.data(), kNormals.size() * sizeof(float));

    // ── Descriptor set ────────────────────────────────────────────────────
    const VkDescriptorSetAllocateInfo ds_alloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout_,
    };
    check_vk(vkAllocateDescriptorSets(device_, &ds_alloc, &descriptor_set_),
             "vkAllocateDescriptorSets");

    const VkDescriptorBufferInfo buf_desc{
        .buffer = buffer_,
        .offset = 0,
        .range = sizeof(Ubo),
    };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set_,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buf_desc,
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    initialised_ = true;
}

void CubeRenderer::Cleanup() {
    if (!initialised_)
        return;
    vkDestroyPipeline(device_, pipeline_, nullptr);
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    vkDestroyDescriptorSetLayout(device_, set_layout_, nullptr);
    vkUnmapMemory(device_, memory_);
    vkDestroyBuffer(device_, buffer_, nullptr);
    vkFreeMemory(device_, memory_, nullptr);
    initialised_ = false;
}

CubeRenderer::~CubeRenderer() {
    Cleanup();
}

void CubeRenderer::AddMouseDelta(float dx, float dy) {
    mouse_yaw_ += dx * 0.5f;
    mouse_pitch_ += dy * 0.5f;
}

void CubeRenderer::AddScrollDelta(float delta) {
    constexpr float kMin = 2.0f;
    constexpr float kMax = 30.0f;
    camera_distance_ = std::clamp(camera_distance_ - delta * 0.5f, kMin, kMax);
}

void CubeRenderer::ResetMouseRotation() {
    mouse_yaw_ = 0.0f;
    mouse_pitch_ = 0.0f;
}

void CubeRenderer::Draw(VkCommandBuffer cmd, uint32_t width, uint32_t height) {
    Ubo ubo{};
    struct timeval tv{};
    gettimeofday(&tv, nullptr);

    const uint64_t t = animate_
                           ? (static_cast<uint64_t>(tv.tv_sec * 1000 + tv.tv_usec / 1000) -
                              static_cast<uint64_t>(start_tv_.tv_sec * 1000 + start_tv_.tv_usec / 1000)) /
                                 5
                           : 0;

    esMatrixLoadIdentity(&ubo.modelview);
    esTranslate(&ubo.modelview, 0.0f, 0.0f, -camera_distance_);

    if (animate_) {
        esRotate(&ubo.modelview, 45.0f + (0.25f * static_cast<float>(t)), 1.0f, 0.0f, 0.0f);
        esRotate(&ubo.modelview, 45.0f - (0.5f * static_cast<float>(t)), 0.0f, 1.0f, 0.0f);
        esRotate(&ubo.modelview, 10.0f + (0.15f * static_cast<float>(t)), 0.0f, 0.0f, 1.0f);
    }

    // Add mouse-accumulated rotation on top
    esRotate(&ubo.modelview, mouse_pitch_, 1.0f, 0.0f, 0.0f);
    esRotate(&ubo.modelview, mouse_yaw_, 0.0f, 1.0f, 0.0f);

    const float aspect = static_cast<float>(height) / static_cast<float>(width);
    // Keep the same FOV as the original (half_w / nearZ = 2.8 / 6.0)
    // but track camera_distance_ so the cube is never outside near/far planes.
    constexpr float kHalfFovTan = 2.8f / 6.0f;
    const float near_z = std::max(0.5f, camera_distance_ - 4.0f);
    const float far_z = camera_distance_ + 4.0f;
    const float half_w = near_z * kHalfFovTan;
    const float half_h = half_w * aspect;
    ESMatrix projection{};
    esMatrixLoadIdentity(&projection);
    esFrustum(&projection, -half_w, +half_w, -half_h, +half_h, near_z, far_z);

    esMatrixLoadIdentity(&ubo.modelviewprojection);
    esMatrixMultiply(&ubo.modelviewprojection, &ubo.modelview, &projection);
    std::memcpy(ubo.normal, &ubo.modelview, sizeof(ubo.normal));

    std::memcpy(map_, &ubo, sizeof(Ubo));

    // Bind pipeline + resources
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

    const std::array<VkBuffer, 3> bufs{{buffer_, buffer_, buffer_}};
    const std::array<VkDeviceSize, 3> offsets{{
        static_cast<VkDeviceSize>(vertex_offset_),
        static_cast<VkDeviceSize>(colors_offset_),
        static_cast<VkDeviceSize>(normals_offset_),
    }};
    vkCmdBindVertexBuffers(cmd, 0, 3, bufs.data(), offsets.data());

    const VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    const VkRect2D scissor{{0, 0}, {width, height}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 6 faces × 4 vertices each (TRIANGLE_STRIP)
    for (uint32_t face = 0; face < 6; ++face)
        vkCmdDraw(cmd, 4, 1, face * 4, 0);
}
