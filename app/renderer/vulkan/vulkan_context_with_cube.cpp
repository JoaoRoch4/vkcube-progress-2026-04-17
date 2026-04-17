#include "vulkan_context_with_cube.hpp"

#include "cube_renderer.hpp"
#include "imgui_impl_vulkan.h"

VulkanContextWithCube::VulkanContextWithCube()
    : m_cube_renderer { nullptr }
    , m_show_cube { nullptr }
{
}

void VulkanContextWithCube::SetCubeRenderer(CubeRenderer* renderer, bool* show_cube)
{
	m_cube_renderer = renderer;
	m_show_cube = show_cube;
}

void VulkanContextWithCube::FrameRender(ImDrawData* draw_data)
{
	ImGui_ImplVulkanH_Window* wd = &MainWindowData;
	VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

	VkResult err = vkAcquireNextImageKHR(Device, wd->Swapchain, UINT64_MAX,
		image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		SwapChainRebuild = true;
	if (err == VK_ERROR_OUT_OF_DATE_KHR)
		return;
	if (err != VK_SUBOPTIMAL_KHR)
		CheckVkResult(err);

	ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
	{
		err = vkWaitForFences(Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
		CheckVkResult(err);
		err = vkResetFences(Device, 1, &fd->Fence);
		CheckVkResult(err);
	}
	{
		err = vkResetCommandPool(Device, fd->CommandPool, 0);
		CheckVkResult(err);
		VkCommandBufferBeginInfo info {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		CheckVkResult(err);
	}
	{
		VkRenderPassBeginInfo info {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = wd->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = static_cast<uint32_t>(wd->Width);
		info.renderArea.extent.height = static_cast<uint32_t>(wd->Height);
		info.clearValueCount = 1;
		info.pClearValues = &wd->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// ── Cube background ───────────────────────────────────────────────────
	if (m_show_cube && *m_show_cube && m_cube_renderer) {
		m_cube_renderer->Draw(fd->CommandBuffer,
			static_cast<uint32_t>(wd->Width),
			static_cast<uint32_t>(wd->Height));
	}
	// ─────────────────────────────────────────────────────────────────────

	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

	vkCmdEndRenderPass(fd->CommandBuffer);

	// ── Screen capture readback ───────────────────────────────────────────
	EnsureReadbackBuffer(wd->Width, wd->Height);
	if (ReadbackBuffer != VK_NULL_HANDLE) {
		VkImageMemoryBarrier to_transfer {};
		to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		to_transfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		to_transfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_transfer.image = fd->Backbuffer;
		to_transfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdPipelineBarrier(fd->CommandBuffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &to_transfer);

		VkBufferImageCopy region {};
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent = { static_cast<uint32_t>(wd->Width), static_cast<uint32_t>(wd->Height), 1 };
		vkCmdCopyImageToBuffer(fd->CommandBuffer, fd->Backbuffer,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ReadbackBuffer, 1, &region);

		VkImageMemoryBarrier to_present = to_transfer;
		to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		to_present.dstAccessMask = 0;
		to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(fd->CommandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &to_present);
	}
	// ─────────────────────────────────────────────────────────────────────

	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;
		err = vkEndCommandBuffer(fd->CommandBuffer);
		CheckVkResult(err);
		err = vkQueueSubmit(Queue, 1, &info, fd->Fence);
		CheckVkResult(err);
		ReadbackLastFence = fd->Fence; // track for ReadPixels fence wait
	}
}
