#include "ImGuiRenderer.hpp"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui_threaded_rendering/imgui_threaded_rendering.h>
#include "Core/Threading/SPSCRing.hpp"

namespace Cori {
	namespace Graphics {
		struct ImGuiRenderer::Data {
			std::array<ImDrawDataSnapshot, FRAMES_IN_FLIGHT + 1> m_Snapshots;
			ImDrawDataSnapshot* m_InFlight{ nullptr };

			Threading::SPSCRing<ImDrawDataSnapshot*> m_ReadyRing{ FRAMES_IN_FLIGHT + 1 };
			Threading::SPSCRing<ImDrawDataSnapshot*> m_RecycleRing{ FRAMES_IN_FLIGHT + 1 };

			ImTextureQueue m_TexQueue;
			std::mutex m_TexQueueMutex;
		};

		std::unique_ptr<ImGuiRenderer::Data> ImGuiRenderer::s_Data{};

		// The ImGui Vulkan backend maps and unmaps its vertex/index buffers every frame multiple times (A LOT!). Each one is a syscall taking system-wide mmap_lock, on my system it lead to stutters under some conditions.
		namespace {
			struct PersistentMappings {
				std::unordered_map<VkDeviceMemory, void*> m_Bases;
				std::mutex m_Mutex;
			};

			PersistentMappings& GetPersistentMappings() {
				static PersistentMappings s_Mappings;
				return s_Mappings;
			}

			VKAPI_ATTR VkResult VKAPI_CALL PersistentMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize, VkMemoryMapFlags flags, void** ppData) {
				auto& mappings = GetPersistentMappings();
				std::lock_guard lk(mappings.m_Mutex);

				auto it = mappings.m_Bases.find(memory);
				if (it == mappings.m_Bases.end()) {
					void* base = nullptr;
					const VkResult result = VULKAN_HPP_DEFAULT_DISPATCHER.vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, flags, &base);
					if (result != VK_SUCCESS) {
						return result;
					}

					it = mappings.m_Bases.emplace(memory, base).first;
				}

				*ppData = static_cast<uint8_t*>(it->second) + offset;
				return VK_SUCCESS;
			}

			VKAPI_ATTR void VKAPI_CALL PersistentUnmapMemory(VkDevice, VkDeviceMemory) {}

			VKAPI_ATTR void VKAPI_CALL PersistentFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
				{
					auto& mappings = GetPersistentMappings();
					std::lock_guard lk(mappings.m_Mutex);
					mappings.m_Bases.erase(memory);
				}

				VULKAN_HPP_DEFAULT_DISPATCHER.vkFreeMemory(device, memory, allocator);
			}

			PFN_vkVoidFunction LoadImGuiVulkanFunction(const char* name, void* userData) {
				const std::string_view function{ name };

				if (function == "vkMapMemory") {
					return reinterpret_cast<PFN_vkVoidFunction>(&PersistentMapMemory);
				}
				if (function == "vkUnmapMemory") {
					return reinterpret_cast<PFN_vkVoidFunction>(&PersistentUnmapMemory);
				}
				if (function == "vkFreeMemory") {
					return reinterpret_cast<PFN_vkVoidFunction>(&PersistentFreeMemory);
				}

				if (PFN_vkVoidFunction deviceFunction = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr(static_cast<VkDevice>(userData), name)) {
					return deviceFunction;
				}

				return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(static_cast<VkInstance>(VulkanEngine::GetInstance()), name);
			}
		}

		void ImGuiRenderer::Init(void* window) {
			CORI_CORE_ASSERT(!s_Data, "ImGuiRenderer::Init called twice.");

			s_Data = std::make_unique<Data>();
			for (auto& inst : s_Data->m_Snapshots) {
				s_Data->m_RecycleRing.Emplace(&inst);
			}

			ImGui_ImplSDL3_InitForVulkan(static_cast<SDL_Window*>(window));

			ImGui_ImplVulkan_InitInfo vulkanInfo{
				.ApiVersion = VK_API_VERSION_1_3,
				.Instance = static_cast<VkInstance>(VulkanEngine::GetInstance()),
				.PhysicalDevice = static_cast<VkPhysicalDevice>(VulkanEngine::GetPhysicalDevice()),
				.Device = static_cast<VkDevice>(VulkanEngine::GetLogicalDevice()),
				.QueueFamily = VulkanEngine::GetGraphicsQueueFamilyIndex(),
				.Queue = static_cast<VkQueue>(VulkanEngine::GetGraphicsQueue()),
				.DescriptorPoolSize = 1000,
				.MinImageCount = 2,
				.ImageCount = VulkanEngine::GetSwapChainImageCount(),
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

			bool functionsLoaded = ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, &LoadImGuiVulkanFunction, static_cast<VkDevice>(VulkanEngine::GetLogicalDevice()));
			CORI_CORE_ASSERT(functionsLoaded, "Failed to load Vulkan functions for the ImGui backend.");

			bool success = ImGui_ImplVulkan_Init(&vulkanInfo);
			CORI_CORE_ASSERT(success, "Failed to initialize ImGui with Vulkan.");

			s_Data->m_TexQueue.UpdateTexFunc = ImGui_ImplVulkan_UpdateTexture;
			s_Data->m_TexQueue.InFlightFrames = FRAMES_IN_FLIGHT;
		}

		void ImGuiRenderer::Shutdown() {
			auto result = VulkanEngine::GetLogicalDevice().waitIdle();
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Calling wait idle on device has failed. Error: {}", vk::to_string(result));

			s_Data->m_TexQueue.Shutdown();
			for (auto& inst : s_Data->m_Snapshots) {
				inst.Clear();
			}

			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplSDL3_Shutdown();
		}

		void ImGuiRenderer::StartFrame() {
			{
				std::lock_guard lk(s_Data->m_TexQueueMutex);
				s_Data->m_TexQueue.PreNewFrame();
			}
			ImGui_ImplSDL3_NewFrame();
		}

		void ImGuiRenderer::EndFrame() {
			CORI_PROFILE_FUNCTION();
			ImDrawData* drawData = ImGui::GetDrawData();
			{
				std::lock_guard lk(s_Data->m_TexQueueMutex);
				s_Data->m_TexQueue.QueueRequests(drawData);
			}

			ImDrawDataSnapshot* snapshot = *s_Data->m_RecycleRing.FrontWait();
			s_Data->m_RecycleRing.Pop();
			snapshot->SnapUsingSwap(drawData, ImGui::GetTime());
			s_Data->m_ReadyRing.Emplace(snapshot);
		}

		void ImGuiRenderer::ProcessTexQueueRequests() {
			CORI_PROFILE_FUNCTION();
			CORI_CORE_ASSERT(!s_Data->m_InFlight, "ProcessTexQueueRequests called but some snapshot was still in flight.");
			s_Data->m_InFlight = *s_Data->m_ReadyRing.FrontWait();
			s_Data->m_ReadyRing.Pop();

			{
				std::lock_guard lk(s_Data->m_TexQueueMutex);
				s_Data->m_TexQueue.ProcessRequests(&s_Data->m_InFlight->DrawData);
			}
		}

		void ImGuiRenderer::Render(vk::CommandBuffer cmb) {
			CORI_PROFILE_FUNCTION();
			CORI_CORE_ASSERT(s_Data->m_InFlight, "Render called but no snapshot is in flight.");
			CORI_PROFILE_GPU_ZONE_CP(Cori::ProfileParts::RenderingLoop, VulkanEngine::GetGraphicsGPUProfilerContext(), cmb, "ImGui Rendering", Cori::ProfileColors::GPUPass);
			CORI_VK_LABEL(cmb, "ImGui Rendering", DebugLabelColors::Pass);

			ImGui_ImplVulkan_RenderDrawData(&s_Data->m_InFlight->DrawData, cmb);

			#if 0
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


				cmb.endRendering();

			}

			//const ImGuiIO& io = ImGui::GetIO();

			//if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			//{
			//	ImGui::UpdatePlatformWindows();
			//	ImGui::RenderPlatformWindowsDefault();
			//}
			#endif
		}

		void ImGuiRenderer::RecycleSnapshot() {
			CORI_CORE_ASSERT(s_Data->m_InFlight, "RecycleSnapshot called but no snapshot is in flight.");
			ImDrawDataSnapshot* ptr = s_Data->m_InFlight;
			s_Data->m_InFlight = nullptr;
			s_Data->m_RecycleRing.Emplace(ptr);
		}
	}
}