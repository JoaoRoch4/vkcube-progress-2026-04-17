#include <array>
#include <bit>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <format>
#include <print>
#include <string>

#include <dlfcn.h>

#include "app/platform/sdl_window.hpp"
#include "app/renderer/vulkan/vulkan_context.hpp"
#include "app/ui/imgui_layer.hpp"

#include "imgui.h"
#include "imgui_impl_vulkan.h"

#include "app/hot_module.h"
#include "app/ui/test_engine_layer.hpp"
#include "cube_renderer.hpp"
#include "imgui_te_engine.h"

// ── Cube background ───────────────────────────────────────────────────────────
//
// When show_cube is true, CubeRenderer::Draw() is called inside the ImGui
// Vulkan render pass (after the clear, before ImGui draw data) so the cube
// renders as the background behind the UI.
//
// Mouse controls:
//   Right-button drag  — rotate cube
//   Middle-button      — reset mouse rotation
//
// ─────────────────────────────────────────────────────────────────────────────

// Extend VulkanContext so we can inject the cube draw call between BeginRenderPass
// and ImGui_ImplVulkan_RenderDrawData.  We do this by subclassing and overriding
// FrameRender.  The override calls the base implementation but hooks in the cube
// via a pre-render callback.
//
// A simpler approach that avoids touching VulkanContext: we store a raw pointer to
// the CubeRenderer here and provide a custom FrameRender wrapper.

static CubeRenderer* g_cube_renderer = nullptr;
static bool g_show_cube = true;
static TestEngineLayer* g_test_engine = nullptr;

// ── Hot-reload module ─────────────────────────────────────────────────────────
struct HotModule {
	using GetApiFn = HotModuleAPI* (*)();

	void* m_handle = nullptr;
	HotModuleAPI* m_api = nullptr;
	std::filesystem::file_time_type m_mtime = {};
	int m_gen = 0;

	// Resolve paths relative to the executable so the app works regardless
	// of the working directory it was launched from.
	static std::filesystem::path exe_dir()
	{
		std::error_code ec;
		auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
		return ec ? std::filesystem::current_path() : p.parent_path();
	}

	static std::filesystem::path src_path() { return exe_dir() / "libhot.so"; }

	void load()
	{
		const auto src = src_path();

		// Copy to a uniquely-named file: Linux caches dlopen by inode,
		// so re-opening the same path returns the old handle.
		m_gen = (m_gen + 1) % 8;
		auto tmp = exe_dir() / std::format("libhot_{}.so", m_gen);

		std::error_code ec;
		std::filesystem::copy_file(src, tmp,
			std::filesystem::copy_options::overwrite_existing, ec);
		if (ec) {
			std::println(stderr, "[hot] copy failed: {}", ec.message());
			return;
		}

		// Unload previous generation.
		if (m_api && m_api->shutdown)
			m_api->shutdown();
		if (m_handle) {
			dlclose(m_handle);
			m_handle = nullptr;
			m_api = nullptr;
		}

		m_handle = dlopen(tmp.c_str(), RTLD_NOW | RTLD_LOCAL);
		if (!m_handle) {
			std::println(stderr, "[hot] dlopen: {}", dlerror());
			return;
		}

		auto fn = std::bit_cast<GetApiFn>(dlsym(m_handle, "hot_get_api"));
		if (!fn) {
			std::println(stderr, "[hot] dlsym: {}", dlerror());
			dlclose(m_handle);
			m_handle = nullptr;
			return;
		}

		m_api = fn();
		if (m_api && m_api->init)
			m_api->init(ImGui::GetCurrentContext());

		m_mtime = std::filesystem::last_write_time(src, ec);
		std::println("[hot] reloaded (gen {})", m_gen);
	}

	void tick()
	{
		std::error_code ec;
		auto t = std::filesystem::last_write_time(src_path(), ec);
		if (!ec && t != m_mtime)
			load();
	}

	void build_ui()
	{
		if (m_api && m_api->build_ui)
			m_api->build_ui();
	}

	void shutdown()
	{
		if (m_api && m_api->shutdown)
			m_api->shutdown();
		if (m_handle) {
			dlclose(m_handle);
			m_handle = nullptr;
		}
		m_api = nullptr;
	}
};

static HotModule* g_hot_module = nullptr;

// ── Copilot ↔ App messaging ────────────────────────────────────────────────────
// Copilot → App: call copilot_say("message") from LLDB evaluate()
// App → Copilot: call copilot_read() from LLDB evaluate() to pop next user msg
// Test control:  call copilot_run_test("category/name") to queue a test

static std::deque<std::string> g_copilot_messages; // inbound (Copilot → App)
static std::deque<std::string> g_user_messages; // outbound (App → Copilot)

extern "C" void copilot_say(const char* msg)
{
	if (msg)
		g_copilot_messages.emplace_back(msg);
}

extern "C" const char* copilot_read()
{
	static std::string s_buf;
	if (g_user_messages.empty())
		return nullptr;
	s_buf = std::move(g_user_messages.front());
	g_user_messages.pop_front();
	return s_buf.c_str();
}

extern "C" void copilot_run_test(const char* filter)
{
	if (g_test_engine && g_test_engine->Engine && filter)
		ImGuiTestEngine_QueueTests(g_test_engine->Engine,
			ImGuiTestGroup_Tests, filter, ImGuiTestRunFlags_None);
}

extern "C" void copilot_reload_hot()
{
	if (g_hot_module)
		g_hot_module->load();
}

// Override FrameRender in a thin subclass.
class VulkanContextWithCube : public VulkanContext {
    public:
	void FrameRender(ImDrawData* draw_data);
};

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
	if (g_show_cube && g_cube_renderer) {
		g_cube_renderer->Draw(fd->CommandBuffer,
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
	// ───────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────

int main()
{
	// 1. Window
	SDLWindow sdl;
	if (!sdl.Init("vkcube + ImGui", 1280, 800)) {
		std::println(stderr, "SDLWindow::Init failed");
		return EXIT_FAILURE;
	}

	// 2. Vulkan (subclass with cube injection)
	VulkanContextWithCube vulkan;
	vulkan.Setup(sdl.GetVulkanExtensions());
	VkSurfaceKHR surface = sdl.CreateVulkanSurface(vulkan.Instance, vulkan.Allocator);
	if (surface == VK_NULL_HANDLE) {
		std::println(stderr, "CreateVulkanSurface failed");
		return EXIT_FAILURE;
	}
	int w {}, h {};
	sdl.GetSize(w, h);
	vulkan.SetupWindow(surface, w, h);
	sdl.Show();

	// 3. Cube renderer — uses the same device/renderpass as ImGui Vulkan backend
	CubeRenderer cube;
	VkPhysicalDeviceMemoryProperties mem_props {};
	vkGetPhysicalDeviceMemoryProperties(vulkan.PhysicalDevice, &mem_props);
	cube.Init(vulkan.Device,
		vulkan.PhysicalDevice,
		mem_props,
		vulkan.Queue,
		vulkan.QueueFamily,
		vulkan.MainWindowData.RenderPass,
		vulkan.DescriptorPool);
	g_cube_renderer = &cube;

	// 4. ImGui
	ImGui_ImplVulkan_InitInfo init_info = vulkan.MakeInitInfo();
	ImGuiLayer imgui;
	imgui.Init(sdl.Window, init_info, sdl.MainScale);

	// 5. Test engine (must be after ImGui::CreateContext)
	TestEngineLayer test_engine;
	test_engine.Init();
	g_test_engine = &test_engine;

	// Wire Vulkan readback to test engine screen capture
	{
		ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(test_engine.Engine);
		test_io.ScreenCaptureFunc = [](ImGuiID, int x, int y, int w, int h,
		                               unsigned int* pixels, void* user_data) -> bool {
			return static_cast<VulkanContext*>(user_data)->ReadPixels(x, y, w, h, pixels);
		};
		test_io.ScreenCaptureUserData = &vulkan;
	}

	// 6. Hot-reload module (initial load — silently skipped if libhot.so absent)
	HotModule hot;
	g_hot_module = &hot;
	hot.load();

	// 7. Main loop
	bool done = false;
	bool rmb_held = false;
	float prev_mouse_x = 0.0f;
	float prev_mouse_y = 0.0f;

	while (!done) {
		sdl.PollEvents(done, [&](const SDL_Event* e) {
			imgui.ProcessEvent(e);

			if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_RIGHT) {
				rmb_held = true;
				prev_mouse_x = e->button.x;
				prev_mouse_y = e->button.y;
			}
			if (e->type == SDL_EVENT_MOUSE_BUTTON_UP && e->button.button == SDL_BUTTON_RIGHT) {
				rmb_held = false;
			}
			if (e->type == SDL_EVENT_MOUSE_MOTION && rmb_held) {
				cube.AddMouseDelta(e->motion.x - prev_mouse_x,
					e->motion.y - prev_mouse_y);
				prev_mouse_x = e->motion.x;
				prev_mouse_y = e->motion.y;
			}
			if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_MIDDLE) {
				cube.ResetMouseRotation();
			}
			if (e->type == SDL_EVENT_MOUSE_WHEEL && g_show_cube) {
				// ImGui may have captured the wheel; only scroll the cube when
				// the cursor is not over a Dear ImGui window.
				if (!ImGui::GetIO().WantCaptureMouse)
					cube.AddScrollDelta(e->wheel.y);
			}
		});

		done = done || imgui.RequestQuit;
		if (sdl.IsMinimized()) {
			SDL_Delay(10);
			continue;
		}

		sdl.GetSize(w, h);
		vulkan.RebuildSwapchainIfNeeded(w, h);
		if (vulkan.SwapChainRebuild)
			continue;

		imgui.NewFrame();
		hot.tick();

		// ── Custom overlay window ─────────────────────────────────────────
		ImGui::Begin("Controls");
		ImGui::Checkbox("Cube background", &g_show_cube);
		if (g_show_cube) {
			bool anim = cube.GetAnimate();
			if (ImGui::Checkbox("Auto-spin", &anim))
				cube.SetAnimate(anim);
			if (ImGui::Button("Reset rotation"))
				cube.ResetMouseRotation();
			ImGui::TextDisabled("Right-drag to rotate, Middle to reset");
		}
		ImGui::Separator();
		ImGui::Text("%.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));
		ImGui::Separator();
		if (ImGui::Button("Reload hot module"))
			hot.load();
		ImGui::End();
		// ─────────────────────────────────────────────────────────────────

		// ── Copilot message window ───────────────────────────────────────
		// Always show so the user can always type messages back.
		{
			static std::array<char, 512> input_buf {};
			ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(ImVec2(20, 200), ImGuiCond_Appearing);
			if (ImGui::Begin("Copilot")) {
				// Messages from Copilot
				for (const auto& msg : g_copilot_messages)
					ImGui::TextWrapped("%s", msg.c_str());
				if (!g_copilot_messages.empty() && ImGui::Button("Clear"))
					g_copilot_messages.clear();
				ImGui::Separator();
				// Input: user → Copilot
				ImGui::SetNextItemWidth(-80.0f);
				bool submitted = ImGui::InputText("##copilot_in",
					input_buf.data(), input_buf.size(),
					ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::SameLine();
				if ((ImGui::Button("Send") || submitted) && input_buf.at(0) != '\0') {
					g_user_messages.emplace_back(input_buf.data());
					input_buf.fill('\0');
					ImGui::SetKeyboardFocusHere(-1);
				}
			}
			ImGui::End();
		}
		// ─────────────────────────────────────────────────────────────────

		// ── Test engine window ───────────────────────────────────────────
		test_engine.BuildUI(&imgui.ShowTestEngineWindow);
		// ─────────────────────────────────────────────────────────────────

		// ── Hot-reload module UI ─────────────────────────────────────────
		hot.build_ui();
		// ─────────────────────────────────────────────────────────────────

		imgui.BuildUI();
		imgui.Render();

		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data->DisplaySize.x > 0.0f && draw_data->DisplaySize.y > 0.0f) {
			if (!g_show_cube)
				vulkan.SetClearColor(imgui.ClearColor);
			else
				vulkan.SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
			vulkan.FrameRender(draw_data);
			vulkan.FramePresent();
			test_engine.PostSwap();
		}
	}

	// Cleanup
	vulkan.WaitIdle();
	g_cube_renderer = nullptr;
	cube.Cleanup();
	hot.shutdown();
	g_hot_module = nullptr;
	test_engine.Stop();
	imgui.Shutdown();
	test_engine.Shutdown();
	g_test_engine = nullptr;
	vulkan.CleanupWindow();
	vulkan.Cleanup();
	sdl.Shutdown();

	return 0;
}
