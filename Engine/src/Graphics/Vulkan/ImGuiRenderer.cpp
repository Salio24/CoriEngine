#include "ImGuiRenderer.hpp"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl3.h>

namespace Cori {
	namespace Graphics {
		void ImGuiRenderer::Init(void* window) {
			ImGui_ImplSDL3_InitForVulkan(static_cast<SDL_Window*>(window));

			ImGui_ImplVulkan_InitInfo vulkanInfo{
				.ApiVersion = VK_API_VERSION_1_3,
				.Instance = static_cast<VkInstance>(VulkanEngine::GetInstance()),
				.PhysicalDevice = static_cast<VkPhysicalDevice>(VulkanEngine::GetPhysicalDevice()),
				.Device = static_cast<VkDevice>(VulkanEngine::GetLogicalDevice()),
				.QueueFamily = VulkanEngine::GetGraphicsQueueFamilyIndex(),
				.Queue = static_cast<VkQueue>(VulkanEngine::GetGraphicsQueue()),
				.DescriptorPoolSize = 1000,
				.MinImageCount = FRAMES_IN_FLIGHT,
				.ImageCount = FRAMES_IN_FLIGHT,
				.UseDynamicRendering = true
			};

			VkFormat colorFormat = static_cast<VkFormat>(VulkanEngine::GetSwapChaimImageFormat().format);

			vulkanInfo.PipelineInfoMain.PipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			vulkanInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pNext = nullptr;
			vulkanInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
			vulkanInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
			vulkanInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
			vulkanInfo.CheckVkResultFn = [](VkResult result) {
				CORI_CORE_ASSERT(result == VK_SUCCESS, "ImGui Vulkan Error: {}", vk::to_string(static_cast<vk::Result>(result)));
			};

			bool success = ImGui_ImplVulkan_Init(&vulkanInfo);
			CORI_CORE_ASSERT(success, "Failed to initialize ImGui with Vulkan.");
		}

		void ImGuiRenderer::Shutdown() {
			auto result = VulkanEngine::GetLogicalDevice().waitIdle();
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Calling wait idle on device has failed. Error: {}", vk::to_string(result));

			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplSDL3_Shutdown();
		}

		void ImGuiRenderer::StartFrame() {
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplSDL3_NewFrame();
		}

		void ImGuiRenderer::Render(vk::CommandBuffer cmb, vk::ImageView swapChainImageView, const vk::Extent2D swapChainExtent, const bool frameSkip) {
			ImDrawData* drawData = ImGui::GetDrawData();
			if (drawData && !frameSkip) {
				vk::RenderingAttachmentInfo colorAttachment = {
					.imageView = swapChainImageView,
					.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.loadOp = vk::AttachmentLoadOp::eLoad,
					.storeOp = vk::AttachmentStoreOp::eStore
				};

				vk::RenderingInfo renderInfo = {
					.renderArea = {{0, 0}, swapChainExtent},
					.layerCount = 1,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachment
				};

				cmb.beginRendering(renderInfo);

				ImGui_ImplVulkan_RenderDrawData(drawData, cmb);

				cmb.endRendering();
			}

			const ImGuiIO& io = ImGui::GetIO();

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}
	}
}