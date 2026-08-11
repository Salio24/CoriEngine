#include "VulkanEngine.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_hpp_macros.hpp>
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#include "VulkanMeshManager.hpp"
#include "VulkanLayoutManager.hpp"
#include "VulkanImageViewManager.hpp"
#include "VulkanShaderManager.hpp"
#include "VulkanTextureManager.hpp"
#include "VulkanMaterialSystem.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "DeletionQueue.hpp"
#include "VmaLeakLog.hpp"
#include "DeviceLossDebug/VulkanDeviceLossDebug.hpp"
#include "Renderer/MasterRenderer.hpp"
#include "Renderer/ThumbnailAtlas.hpp"

const std::vector g_ValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

std::vector<const char*> Cori::Graphics::VulkanEngine::m_DeviceExtensions;
std::vector<const char*> Cori::Graphics::VulkanEngine::m_OptionalDeviceExtensions;
std::vector<const char*> Cori::Graphics::VulkanEngine::m_EnabledDeviceExtensions;
std::vector<const char*> Cori::Graphics::VulkanEngine::m_InstanceExtensions;
bool Cori::Graphics::VulkanEngine::s_VerboseValidationLayerLogging = false;

#if defined(DEBUG_BUILD) && !defined(CORI_ENABLE_PROFILING)
bool Cori::Graphics::VulkanEngine::s_EnableDeviceAddressBindingReport = true;
#else
bool Cori::Graphics::VulkanEngine::s_EnableDeviceAddressBindingReport = false;
#endif

static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
	if (!!(type & vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding)) {
		bool bindingHandled = false;

		for (const auto* next = static_cast<const vk::BaseInStructure*>(pCallbackData->pNext); next != nullptr; next = next->pNext) {
			if (next->sType != vk::StructureType::eDeviceAddressBindingCallbackDataEXT) {
				continue;
			}

			const auto& binding = *reinterpret_cast<const vk::DeviceAddressBindingCallbackDataEXT*>(next);

			const bool unbind = binding.bindingType == vk::DeviceAddressBindingTypeEXT::eUnbind;
			const bool internalObject = !!(binding.flags & vk::DeviceAddressBindingFlagBitsEXT::eInternalObject);

			vk::ObjectType objectType = vk::ObjectType::eUnknown;
			uint64_t objectHandle = 0;

			if (pCallbackData->objectCount > 0) {
				objectType = pCallbackData->pObjects[0].objectType;
				objectHandle = pCallbackData->pObjects[0].objectHandle;
			}

			Cori::Graphics::VulkanDeviceLossDebug::RecordAddressBinding(binding.baseAddress, binding.size, unbind, internalObject, objectType, objectHandle);

			bindingHandled = true;
		}

		if (bindingHandled) {
			return vk::False;
		}
	}

	std::stringstream message;

	message << "Vulkan validation layer: "
		<< "Severity: " << vk::to_string(severity) << " | "
		<< "Type: " << vk::to_string(type) << " | "
		<< "ID: " << pCallbackData->pMessageIdName << " (" << pCallbackData->messageIdNumber << ")" << std::endl;
	message << "Message: " << pCallbackData->pMessage << std::endl;

	if (pCallbackData->objectCount > 0) {
		message << "\tObjects (" << pCallbackData->objectCount << "):" << std::endl;
		for (uint32_t i = 0; i < pCallbackData->objectCount; ++i) {
			const auto& object = pCallbackData->pObjects[i];
			message << "\t\t[" << i << "] Object Type: " << vk::to_string(object.objectType) << std::endl;
			message << "\t\t     Object Handle: " << reinterpret_cast<void*>(object.objectHandle) << std::endl;
			if (object.pObjectName) {
				message << "\t\t     Object Name: \"" << object.pObjectName << "\"" << std::endl;
			}
		}
	}

	switch (severity) {
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
		{
			if (Cori::Graphics::VulkanEngine::s_VerboseValidationLayerLogging) {
				CORI_CORE_DEBUG_TAGGED({ Cori::Logger::Tags::Graphics::Self, Cori::Logger::Tags::Graphics::Vulkan::Self, Cori::Logger::Tags::Graphics::Vulkan::ValidationLayers }, "{}", message.str());
			}
			break;
		}
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
		{
			if (Cori::Graphics::VulkanEngine::s_VerboseValidationLayerLogging) {
				CORI_CORE_INFO_TAGGED({ Cori::Logger::Tags::Graphics::Self, Cori::Logger::Tags::Graphics::Vulkan::Self, Cori::Logger::Tags::Graphics::Vulkan::ValidationLayers }, "{}", message.str());
			}
			break;
		}
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
		{
			CORI_CORE_WARN_TAGGED({ Cori::Logger::Tags::Graphics::Self, Cori::Logger::Tags::Graphics::Vulkan::Self, Cori::Logger::Tags::Graphics::Vulkan::ValidationLayers }, "{}", message.str());
			break;
		}
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
		{
			CORI_CORE_ERROR_TAGGED({ Cori::Logger::Tags::Graphics::Self, Cori::Logger::Tags::Graphics::Vulkan::Self, Cori::Logger::Tags::Graphics::Vulkan::ValidationLayers }, "{}", message.str());
			break;
		}
	}

	return vk::False;
}

namespace Cori {
	namespace Graphics {
		VulkanEngine* VulkanEngine::s_Instance{ nullptr };

		VulkanEngine::VulkanEngine(void* window, const vk::Extent2D swapChainExtent) {
			s_Instance = this;
			PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
			VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

			m_WaitSemaphores.reserve(4);

			m_Window = window;
			m_ValidationLayers = false;
				#if defined(DEBUG_BUILD) && !defined(CORI_ENABLE_PROFILING)
			m_ValidationLayers = g_RendererConfig.ValidationLayers;
				#endif
			m_DeviceExtensions.push_back(vk::KHRSwapchainExtensionName);
			m_DeviceExtensions.push_back(vk::EXTMemoryPriorityExtensionName);
			m_DeviceExtensions.push_back(vk::EXTMemoryBudgetExtensionName);
			m_DeviceExtensions.push_back(vk::EXTShaderObjectExtensionName);
			m_DeviceExtensions.push_back(vk::KHRRobustness2ExtensionName);
			//m_DeviceExtensions.push_back(vk::KHRPushDescriptorExtensionName);
			//m_DeviceExtensions.push_back(vk::KHRUnifiedImageLayoutsExtensionName);
			m_DeviceExtensions.push_back(vk::EXTExtendedDynamicState3ExtensionName);
			m_DeviceExtensions.push_back(vk::EXTDescriptorBufferExtensionName);
				#ifdef CORI_ENABLE_PROFILING
			m_DeviceExtensions.push_back(vk::EXTCalibratedTimestampsExtensionName);
				#endif

			VulkanDeviceLossDebug::Init();
			VulkanPresentTiming::RequestExtensions();
			VulkanPresentTiming::SetPresentQueueDepth(2);

			m_OptionalDeviceExtensions.push_back(vk::EXTDeviceFaultExtensionName);

			if (s_EnableDeviceAddressBindingReport) {
				m_OptionalDeviceExtensions.push_back(vk::EXTDeviceAddressBindingReportExtensionName);
			}

			#ifdef CORI_VK_DL_DEBUG_AMD
			m_OptionalDeviceExtensions.push_back(vk::AMDBufferMarkerExtensionName);
			#endif

			m_InstanceExtensions.push_back(vk::EXTDebugUtilsExtensionName);

			m_NewWindowSizeX.store(swapChainExtent.width, std::memory_order_relaxed);
			m_NewWindowSizeY.store(swapChainExtent.height, std::memory_order_relaxed);

			CreateInstance();
			SetupDebugMessenger();
			CreateSurface();
			PickPhysicalDevice();
			CreateDevice();
			CreateGPUProfilerContexts();
			InitializeVMA();
			CreateSwapChain();
			CreateCommandPools();
			CreateCommandBuffer();
			CreateSyncObjects();

			VulkanDeviceLossDebug::InitDeviceResources();

			ImGuiRenderer::Init(m_Window);
			VulkanImageViewManager::Init();
			DeletionQueue::Init();
			VulkanStreamingLine::Init();
			VulkanMeshManager::Init();
			VulkanGlobalLayoutManager::Init();
			VulkanShaderManager::Init();
			VulkanTextureManager::Init();
			VulkanShaderEffectManager::Init();
			VulkanMaterialSystem::Init();
			VulkanVirtualBufferAllocator::Init();
			VulkanDynamicContainerUploadManager::Init();
			ThumbnailAtlas::Init();
			MasterRenderer::Init();
		}

		VulkanEngine::~VulkanEngine() {
			MasterRenderer::Shutdown();

			auto result = m_Device.waitIdle();
			VulkanDeviceLossDebug::CheckResult(result, "vkDeviceWaitIdle during VulkanEngine teardown");
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Calling wait idle on device has failed. Error: {}", vk::to_string(result));

			VulkanDynamicContainerUploadManager::Shutdown();
			VulkanVirtualBufferAllocator::Shutdown();
			VulkanMaterialSystem::Shutdown();
			VulkanShaderEffectManager::Shutdown();
			VulkanTextureManager::Shutdown();
			VulkanShaderManager::Shutdown();
			VulkanGlobalLayoutManager::Shutdown();
			VulkanMeshManager::Shutdown();
			VulkanStreamingLine::Shutdown();
			ThumbnailAtlas::Shutdown();
			DeletionQueue::Shutdown();
			ImGuiRenderer::Shutdown();
			VulkanImageViewManager::Shutdown();

			VulkanDeviceLossDebug::Shutdown();
			VulkanPresentTiming::Shutdown();

			if (m_TransferGPUProfilerContext != m_GraphicsGPUProfilerContext) {
				CORI_PROFILE_GPU_CONTEXT_DESTROY(m_TransferGPUProfilerContext);
			}
			m_TransferGPUProfilerContext = nullptr;
			CORI_PROFILE_GPU_CONTEXT_DESTROY(m_GraphicsGPUProfilerContext);

			for (auto& semaphore : m_RenderFinishedSemaphores) {
				m_Device.destroySemaphore(semaphore);
			}

			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++ ) {
				FrameInfo& frameData = m_FrameData[i];
				m_Device.destroySemaphore(frameData.m_PresentCompleteSemaphore);
				m_Device.destroyFence(frameData.m_DrawFence);
				m_Device.freeCommandBuffers(m_GraphicsCommandPool, 1, &frameData.m_CommandBuffer);
			}

			m_Device.destroyCommandPool(m_GraphicsCommandPool);
			m_Device.destroyCommandPool(m_TransferCommandPool);

			for (auto& imageView : m_SwapChainImageViews) {
				m_Device.destroyImageView(imageView);
			}

			m_Device.destroySwapchainKHR(m_SwapChain);
			m_Instance.destroySurfaceKHR(m_Surface);

			VmaLeakLog::DumpLiveAllocations("VulkanEngine teardown");
			m_Allocator.destroy();
			VmaLeakLog::LogCollectedLeaks("VulkanEngine teardown");

			m_Device.destroy();
			m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger);
			m_Instance.destroy();
			s_Instance = nullptr;
		}

		void VulkanEngine::Start(void* window, const vk::Extent2D swapChainExtent) {
			CORI_CORE_ASSERT(!s_Instance, "VulkanEngine is already initialized.");
			new VulkanEngine(window, swapChainExtent);
		}

		void VulkanEngine::Stop() {
			delete s_Instance;
		}

		void VulkanEngine::EnterThreadedMode() {
			MasterRenderer::Get().EnterThreadedMode();
		}

		void VulkanEngine::ExitThreadedMode() {
			MasterRenderer::Get().ExitThreadedMode();
		}

		const std::pair<vk::SharingMode, std::vector<uint32_t>>& VulkanEngine::GetBufferSharingSettings(const QueueUsageFlags usage) {
			CORI_CORE_ASSERT(Get().m_SharingSettings.contains(static_cast<QueueUsageFlags::MaskType>(usage)), "Incorrect QueueUsageFlags were passed to VulkanEngine::GetBufferSharingSettings.");
			return Get().m_SharingSettings.at(static_cast<QueueUsageFlags::MaskType>(usage));
		}

		void VulkanEngine::CPUFrameStart() {
			CORI_PROFILE_FUNCTION();

			m_CurrentFrameIndex++;
			VulkanVirtualBufferAllocator::ClearGPUScratchBlock(GetNextFrameInFlight());
		}

		VulkanEngine::FrameInfo& VulkanEngine::GPUFrameBegin() {
			CORI_PROFILE_FUNCTION();

			FrameInfo& frameData = m_FrameData[m_CurrentFrameInFlight];
			frameData.m_FrameIndex = m_CurrentFrameIndex;
			frameData.m_SwapChainImageIndex = UINT32_MAX;
			frameData.m_SkippedFrame = false;

			{
				CORI_PROFILE_SCOPE("Wait for Fences");

				vk::Result fenceResult = vk::Result::eTimeout;
				while (vk::Result::eTimeout == (fenceResult = m_Device.waitForFences(frameData.m_DrawFence, vk::True, UINT64_MAX))) {}

				VulkanDeviceLossDebug::CheckResult(fenceResult, "vkWaitForFences on the draw fence");
				CORI_CORE_ASSERT(fenceResult == vk::Result::eSuccess, "Waiting on the draw fence has failed. Error: {}", vk::to_string(fenceResult));
			}

			{
				CORI_PROFILE_SCOPE("GPU Profiler Collect");
				CORI_PROFILE_GPU_COLLECT(m_GraphicsGPUProfilerContext);
				if (m_TransferGPUProfilerContext != m_GraphicsGPUProfilerContext) {
					CORI_PROFILE_GPU_COLLECT(m_TransferGPUProfilerContext);
				}
			}

			if (m_NewWindowSizeX.load(std::memory_order_acquire) != m_SwapChainExtent.width || m_NewWindowSizeY.load(std::memory_order_acquire) != m_SwapChainExtent.height) {
				ResizeSwapChain();
				frameData.m_SkippedFrame = true;
				return frameData;
			}

			vk::Result result_;
			uint32_t imageIndex;
			{
				CORI_PROFILE_SCOPE("Acquire image");
				auto [result__, imageIndex_] = m_Device.acquireNextImageKHR(m_SwapChain, UINT64_MAX, frameData.m_PresentCompleteSemaphore, nullptr);
				result_ = result__;
				imageIndex = imageIndex_;
			}

			if (result_ == vk::Result::eErrorOutOfDateKHR) {
				ResizeSwapChain();
				frameData.m_SkippedFrame = true;
				return frameData;
			}

			VulkanDeviceLossDebug::CheckResult(result_, "vkAcquireNextImageKHR");

			auto result = m_Device.resetFences(frameData.m_DrawFence);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to reset fence. Error: {}", vk::to_string(result));

			AddWaitSemaphore(frameData.m_PresentCompleteSemaphore, vk::PipelineStageFlagBits::eColorAttachmentOutput);

			frameData.m_SwapChainImageIndex = imageIndex;

			CORI_CORE_ASSERT(result_ == vk::Result::eSuccess || result_ == vk::Result::eSuboptimalKHR, "Failed acquire swapchain image. Error: {}", vk::to_string(result));

			m_CurrentSwapChainImageIndex = imageIndex;

			vk::CommandBufferBeginInfo beginInfo = { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
			result = frameData.m_CommandBuffer.begin(beginInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin command buffer recording. Error: {}", vk::to_string(result));

			CORI_PROFILE_GPU_SPAN_BEGIN_C(m_GPUFrameZone, m_GraphicsGPUProfilerContext, frameData.m_CommandBuffer, "GPU Frame", Cori::ProfileColors::GPUFrame);

			CORI_VK_LABEL_BEGIN_F(frameData.m_CommandBuffer, DebugLabelColors::Frame, "Frame {} (slot {}, swapchain image {})", m_CurrentFrameIndex, m_CurrentFrameInFlight, imageIndex);

			VulkanDeviceLossDebug::BeginFrame(m_CurrentFrameIndex);

			#if 0
			vk::ImageMemoryBarrier2 barrier {
				.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				.srcAccessMask = vk::AccessFlagBits2::eNone,
				.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = m_SwapChainImages[m_CurrentSwapChainImageIndex],
				.subresourceRange = {
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			vk::DependencyInfo depInfo{
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &barrier
			};

			frameData.m_CommandBuffer.pipelineBarrier2(depInfo);
			#endif

			frameData.m_CommandBuffer.setViewportWithCount(vk::Viewport(0.0f, 0.0f, static_cast<float>(m_SwapChainExtent.width), static_cast<float>(m_SwapChainExtent.height), 0.0f, 1.0f));
			frameData.m_CommandBuffer.setScissorWithCount(vk::Rect2D(vk::Offset2D(0, 0), m_SwapChainExtent));
			frameData.m_CommandBuffer.setRasterizerDiscardEnable(vk::False);

			vk::ColorBlendEquationEXT cbeEXT{};
			frameData.m_CommandBuffer.setColorBlendEquationEXT(0, 1, &cbeEXT);

			frameData.m_CommandBuffer.setVertexInputEXT({}, {});

			frameData.m_CommandBuffer.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
			frameData.m_CommandBuffer.setPrimitiveRestartEnable(vk::False);
			frameData.m_CommandBuffer.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);

			vk::SampleMask sampleMask = 0x1;
			frameData.m_CommandBuffer.setSampleMaskEXT(vk::SampleCountFlagBits::e1, &sampleMask);

			frameData.m_CommandBuffer.setAlphaToCoverageEnableEXT(vk::False);

			frameData.m_CommandBuffer.setPolygonModeEXT(vk::PolygonMode::eFill);

			frameData.m_CommandBuffer.setCullMode(vk::CullModeFlagBits::eNone);
			frameData.m_CommandBuffer.setFrontFace(vk::FrontFace::eCounterClockwise);

			frameData.m_CommandBuffer.setDepthTestEnable(vk::False);
			frameData.m_CommandBuffer.setDepthWriteEnable(vk::False);
			frameData.m_CommandBuffer.setDepthCompareOp(vk::CompareOp::eGreater);
			frameData.m_CommandBuffer.setDepthBoundsTestEnable(vk::False);
			frameData.m_CommandBuffer.setDepthBiasEnable(vk::False);
			frameData.m_CommandBuffer.setStencilTestEnable(vk::False);

			frameData.m_CommandBuffer.setLogicOpEnableEXT(vk::False);

			frameData.m_CommandBuffer.setColorBlendEnableEXT(0, 0u);

			vk::ColorComponentFlags ccFlags = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			frameData.m_CommandBuffer.setColorWriteMaskEXT(0, 1, &ccFlags);

			return frameData;
		}

		void VulkanEngine::GPUFrameMiddlePointSync() {
			CORI_PROFILE_FUNCTION();

			FrameInfo& frameData = m_FrameData[m_CurrentFrameInFlight];

			{
				CORI_PROFILE_GPU_ZONE_C(m_GraphicsGPUProfilerContext, frameData.m_CommandBuffer, "Frame Sync Point", Cori::ProfileColors::GPUSync);
				CORI_VK_LABEL(frameData.m_CommandBuffer, "Frame Sync Point", DebugLabelColors::Sync);

				VulkanTextureManager::ProcessUpdates(frameData.m_CommandBuffer);
				VulkanMeshManager::ProcessUpdates(frameData.m_CommandBuffer);
				VulkanShaderEffectManager::Sync();
				VulkanMaterialSystem::Sync();
				VulkanGlobalLayoutManager::Sync();


				VulkanStreamingLine::ProcessUploads();
				VulkanVirtualBufferAllocator::SubmitCopies(frameData.m_CommandBuffer);
				VulkanDynamicContainerUploadManager::ProcessUpdates(frameData.m_CommandBuffer);
			}
		}


		void VulkanEngine::GPUFrameEnd(const FrameLatencyStamps& latencyStamps) {
			CORI_PROFILE_FUNCTION();

			FrameInfo& frameData = m_FrameData[m_CurrentFrameInFlight];

			if (!frameData.m_SkippedFrame) {
				#if 0
				vk::ImageMemoryBarrier2 barrier {
					.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
					.dstAccessMask = {},
					.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.newLayout = vk::ImageLayout::ePresentSrcKHR,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = m_SwapChainImages[m_CurrentSwapChainImageIndex],
					.subresourceRange = {
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};

				vk::DependencyInfo depInfo{
					.imageMemoryBarrierCount = 1,
					.pImageMemoryBarriers = &barrier
				};

				frameData.m_CommandBuffer.pipelineBarrier2(depInfo);
				#endif

				CORI_PROFILE_GPU_SPAN_END(m_GPUFrameZone);

				CORI_VK_LABEL_END(frameData.m_CommandBuffer);

				auto result = frameData.m_CommandBuffer.end();

				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end command buffer recording. Error: {}", vk::to_string(result));

				vk::TimelineSemaphoreSubmitInfo timelineInfo{
					.waitSemaphoreValueCount = static_cast<uint32_t>(m_TimelineWaitValues.size()),
					.pWaitSemaphoreValues = m_TimelineWaitValues.data()
				};

				vk::SubmitInfo submitInfo{
					.waitSemaphoreCount = static_cast<uint32_t>(m_WaitSemaphores.size()),
					.pWaitSemaphores = m_WaitSemaphores.data(),
					.pWaitDstStageMask = m_WaitDstStageMasks.data(),
					.commandBufferCount = 1,
					.pCommandBuffers = &frameData.m_CommandBuffer,
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = &m_RenderFinishedSemaphores[m_CurrentSwapChainImageIndex]
				};

				if (m_TimelineSemaphoresPresent) {
					submitInfo.pNext = &timelineInfo;
				}

				CORI_VK_QUEUE_LABEL_INSERT_F(m_GraphicsQueue, DebugLabelColors::Frame, "Submit frame {}", frameData.m_FrameIndex);

				result = m_GraphicsQueue.submit(submitInfo, frameData.m_DrawFence);
				VulkanDeviceLossDebug::CheckResult(result, "vkQueueSubmit on the graphics queue");
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Graphics queue submission failed. Error: {}", vk::to_string(result));

				vk::PresentInfoKHR presentInfoKHR{
					.pNext = VulkanPresentTiming::PreparePresent(latencyStamps),
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentSwapChainImageIndex],
					.swapchainCount = 1,
					.pSwapchains = &m_SwapChain,
					.pImageIndices = &m_CurrentSwapChainImageIndex
				};

				result = m_PresentQueue.presentKHR(presentInfoKHR);

				VulkanPresentTiming::FinishPresent(result);
				VulkanDeviceLossDebug::CheckResult(result, "vkQueuePresentKHR");

				if (result == vk::Result::eErrorOutOfDateKHR) {
					ResizeSwapChain();
				} else if (result == vk::Result::eSuboptimalKHR) {
					if (m_NewWindowSizeX.load(std::memory_order_relaxed) != m_SwapChainExtent.width || m_NewWindowSizeY.load(std::memory_order_relaxed) != m_SwapChainExtent.height) {
						ResizeSwapChain();
					}
				} else if (result != vk::Result::eSuccess) {
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Presentation failed. Error: {}", vk::to_string(result));
				} else {
					VulkanPresentTiming::Poll();
				}
			}

			m_WaitSemaphores.clear();
			m_WaitDstStageMasks.clear();
			m_TimelineWaitValues.clear();
			m_TimelineSemaphoresPresent = false;
			m_CurrentFrameInFlight = (m_CurrentFrameInFlight + 1) % FRAMES_IN_FLIGHT;
		}

		void VulkanEngine::CreateInstance() {
			uint32_t extensionCount = 0;
			const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
			CORI_CORE_ASSERT(extensionNames, "Failed to get required instance extensions from SDL3. Vulkan instance can't be created. SDL_Error: {}", SDL_GetError());

			for (uint32_t i = 0; i < extensionCount; ++i) {
				m_InstanceExtensions.push_back(extensionNames[i]);
			}

			{
				auto [result, extensionProperties] = m_Context.enumerateInstanceExtensionProperties();

				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to enumerate available instance extensions. Vulkan instance can't be created.")

				for (const auto& extension :  m_InstanceExtensions) {
					CORI_CORE_ASSERT(!std::ranges::none_of(extensionProperties, [extension](auto const& extensionProperty) {
						return strcmp(extensionProperty.extensionName, extension) == 0;
					}), "Required instance extension '{}' is not supported. Vulkan instance can't be created.", extension);
				}
			}

			std::vector<char const*> requiredLayers;
			{
				if (m_ValidationLayers) {
					requiredLayers.assign(g_ValidationLayers.begin(), g_ValidationLayers.end());
				}

				auto [result, layerProperties] = m_Context.enumerateInstanceLayerProperties();

				for (const auto& layer :  requiredLayers) {
					CORI_CORE_ASSERT(!std::ranges::none_of(layerProperties, [layer](auto const& layerProperty) {
						return strcmp(layerProperty.layerName, layer) == 0;
					}), "Required layer '{}' is not supported. Vulkan instance can't be created.", layer);
				}
			}

			constexpr vk::ApplicationInfo appInfo{
				.pApplicationName   = "Cori Application",
				.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
				.pEngineName        = "Cori Engine",
				.engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
				.apiVersion         = vk::ApiVersion13
			};

			vk::InstanceCreateInfo createInfo{
				.pApplicationInfo = &appInfo,
				.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
				.ppEnabledLayerNames = requiredLayers.data(),
				.enabledExtensionCount = static_cast<uint32_t>(m_InstanceExtensions.size()),
				.ppEnabledExtensionNames = m_InstanceExtensions.data()
			};

			VkInstance instance;

			auto result = static_cast<vk::Result>(vkCreateInstance(&*createInfo, nullptr, &instance));
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Vulkan instance can't be created. Error: {}", vk::to_string(result));

			m_Instance = vk::Instance(instance);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Instance);

			#ifdef DEBUG_BUILD
			VulkanDebugLabels::Enable();
			#endif

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Vulkan instance has been created.");
		}

		void VulkanEngine::SetupDebugMessenger() {
			vk::DebugUtilsMessageSeverityFlagsEXT severityFlags( vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError );
			vk::DebugUtilsMessageTypeFlagsEXT    messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding );
			vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
				.messageSeverity = severityFlags,
				.messageType = messageTypeFlags,
				.pfnUserCallback = &DebugCallback
			};

			auto [result, debugMessenger] = m_Instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create debug messenger. Error: {}", vk::to_string(result));
			m_DebugMessenger = debugMessenger;
		}

		void VulkanEngine::CreateSurface() {
			VkSurfaceKHR surface;
			bool success = SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(m_Window), &*m_Instance, nullptr, &surface);
			CORI_CORE_ASSERT(success, "Failed to create Vulkan surface. SDL_Error: {}", SDL_GetError());

			m_Surface = vk::SurfaceKHR(surface);
		}

		void VulkanEngine::PickPhysicalDevice() {
			auto [result, devices] = m_Instance.enumeratePhysicalDevices();
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to enumerate available physical devices. Error: {}", vk::to_string(result));

			bool deviceFound = false;

			std::vector<uint32_t> scores;

			for (const auto& device : devices) {
				uint32_t score = 0;
				auto properties = device.getProperties();
				bool vk13support = properties.apiVersion >= VK_API_VERSION_1_3;

				auto queueFamilies = device.getQueueFamilyProperties();
				bool supportsGraphics =
				std::ranges::any_of(queueFamilies, [](auto const& qfp) {
					return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
				});

				auto [result_, availableDeviceExtensions] = device.enumerateDeviceExtensionProperties();
				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to enumerate available physical device extensions. Error: {}", vk::to_string(result_));

				bool supportsAllRequiredExtensions = std::ranges::all_of(m_DeviceExtensions, [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
					bool supported = std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](auto const& availableDeviceExtension) {
						return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
					});

					//if (!supported) {
					//	CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "{}", requiredDeviceExtension);
					//}

					return supported;
				});

				auto features = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT, vk::PhysicalDeviceMemoryPriorityFeaturesEXT, vk::PhysicalDeviceShaderObjectFeaturesEXT, vk::PhysicalDeviceDescriptorBufferFeaturesEXT, vk::PhysicalDeviceRobustness2FeaturesKHR>();
				bool supportsRequiredFeatures =
					features.get<vk::PhysicalDeviceFeatures2>().features.multiDrawIndirect &&
					features.get<vk::PhysicalDeviceFeatures2>().features.fillModeNonSolid &&
					features.get<vk::PhysicalDeviceFeatures2>().features.wideLines &&
					features.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
					features.get<vk::PhysicalDeviceFeatures2>().features.shaderInt64 &&
					features.get<vk::PhysicalDeviceFeatures2>().features.robustBufferAccess &&
					features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
					features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
					features.get<vk::PhysicalDeviceVulkan12Features>().drawIndirectCount &&
					features.get<vk::PhysicalDeviceVulkan12Features>().descriptorIndexing &&
					features.get<vk::PhysicalDeviceVulkan12Features>().runtimeDescriptorArray &&
					features.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingPartiallyBound &&
					features.get<vk::PhysicalDeviceVulkan12Features>().shaderStorageBufferArrayNonUniformIndexing &&
					features.get<vk::PhysicalDeviceVulkan12Features>().shaderSampledImageArrayNonUniformIndexing &&
					features.get<vk::PhysicalDeviceVulkan12Features>().shaderStorageImageArrayNonUniformIndexing &&
					features.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingStorageBufferUpdateAfterBind &&
					features.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingSampledImageUpdateAfterBind &&
					features.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingStorageImageUpdateAfterBind &&
					features.get<vk::PhysicalDeviceVulkan12Features>().samplerFilterMinmax &&
					features.get<vk::PhysicalDeviceVulkan12Features>().timelineSemaphore &&
					features.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress &&
					features.get<vk::PhysicalDeviceVulkan12Features>().scalarBlockLayout &&
					features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
					features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
					features.get<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>().memoryPriority &&
					features.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>().shaderObject &&
					features.get<vk::PhysicalDeviceDescriptorBufferFeaturesEXT>().descriptorBuffer &&
					features.get<vk::PhysicalDeviceRobustness2FeaturesKHR>().robustBufferAccess2 &&
					features.get<vk::PhysicalDeviceRobustness2FeaturesKHR>().robustImageAccess2 &&
					features.get<vk::PhysicalDeviceRobustness2FeaturesKHR>().nullDescriptor;

				if (vk13support && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures) {
					score+= 1000;
					if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
						score+= 1000;
					} else if (properties.deviceType == vk::PhysicalDeviceType::eVirtualGpu) {
						score+= 500;
					}

					deviceFound = true;
				}

				scores.push_back(score);
			}

			auto it = std::ranges::max_element(scores);
			if (it != scores.end() && *it != 0) {
				uint32_t index = std::distance(scores.begin(), it);
				m_PhysicalDevice = devices[index];
			}

			CORI_CORE_ASSERT(deviceFound, "Failed to find suitable physical device.");

			m_PhysicalDeviceProperties = m_PhysicalDevice.getProperties();
		}

		void VulkanEngine::CreateDevice() {
			std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_PhysicalDevice.getQueueFamilyProperties();

			auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const& qfp) {
				return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
			});

			auto transferQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const& qfp) {
				bool hasGraphics = (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
				bool hasTransfer = (qfp.queueFlags & vk::QueueFlagBits::eTransfer) != static_cast<vk::QueueFlags>(0);
				return !hasGraphics && hasTransfer;
			});

			if (transferQueueFamilyProperty != queueFamilyProperties.end()) {
				m_TransferQueueFamilyIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), transferQueueFamilyProperty));
			} else {
				m_TransferQueueFamilyIndex = m_GraphicsQueueFamilyIndex;
			}

			m_GraphicsQueueFamilyIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

			auto [result, presentSupported] = m_PhysicalDevice.getSurfaceSupportKHR(m_GraphicsQueueFamilyIndex, &*m_Surface);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Query vkGetPhysicalDeviceSurfaceSupportKHR failed. Error: {}", vk::to_string(result));

			m_PresentQueueFamilyIndex = presentSupported ? m_GraphicsQueueFamilyIndex : static_cast<uint32_t>(queueFamilyProperties.size());

			if (m_PresentQueueFamilyIndex == queueFamilyProperties.size()) {
				for (size_t i = 0; i < queueFamilyProperties.size(); ++i) {
					auto [result_, presentSupported__] = m_PhysicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), &*m_Surface);
					CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Query vkGetPhysicalDeviceSurfaceSupportKHR failed. Error: {}", vk::to_string(result_));

					if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics && presentSupported__) {
						m_GraphicsQueueFamilyIndex = static_cast<uint32_t>(i);
						m_PresentQueueFamilyIndex = m_GraphicsQueueFamilyIndex;
						break;
					}
				}
				if (m_PresentQueueFamilyIndex == queueFamilyProperties.size()) {
					for (size_t i = 0; i < queueFamilyProperties.size(); ++i) {
						auto [result_, presentSupported___] = m_PhysicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), &*m_Surface);
						CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Query vkGetPhysicalDeviceSurfaceSupportKHR failed. Error: {}", vk::to_string(result_));
						if (presentSupported___) {
							m_PresentQueueFamilyIndex = static_cast<uint32_t>(i);
							m_DedicatedPresentQueue = true;
							break;
						}
					}
				}
			}

			CORI_CORE_ASSERT(!(m_GraphicsQueueFamilyIndex == queueFamilyProperties.size() || m_PresentQueueFamilyIndex == queueFamilyProperties.size()), "Could not find a queue for graphics or present -> terminating");

			m_NvidiaInstrumentation = VulkanDeviceLossDebug::IsNvidiaInstrumentationSupported(m_PhysicalDevice);

			if (m_NvidiaInstrumentation) {
				m_OptionalDeviceExtensions.push_back(vk::NVDeviceDiagnosticCheckpointsExtensionName);
				m_OptionalDeviceExtensions.push_back(vk::NVDeviceDiagnosticsConfigExtensionName);
			}

			m_EnabledDeviceExtensions = m_DeviceExtensions;
			{
				auto [extensionResult, availableDeviceExtensions] = m_PhysicalDevice.enumerateDeviceExtensionProperties();
				CORI_CORE_ASSERT(extensionResult == vk::Result::eSuccess, "Failed to enumerate available physical device extensions. Error: {}", vk::to_string(extensionResult));

				for (const auto& optionalExtension : m_OptionalDeviceExtensions) {
					const bool supported = std::ranges::any_of(availableDeviceExtensions, [optionalExtension](auto const& availableDeviceExtension) {
						return strcmp(availableDeviceExtension.extensionName, optionalExtension) == 0;
					});

					if (supported) {
						m_EnabledDeviceExtensions.push_back(optionalExtension);
						CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Optional device extension '{}' is supported and will be enabled.", optionalExtension);
					} else {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Optional device extension '{}' is not supported, the tooling relying on it will be unavailable.", optionalExtension);
					}
				}
			}

			const bool deviceFaultEnabled = IsDeviceExtensionEnabled(vk::EXTDeviceFaultExtensionName);

			bool addressBindingReportEnabled = IsDeviceExtensionEnabled(vk::EXTDeviceAddressBindingReportExtensionName);

			if (addressBindingReportEnabled) {
				auto addressBindingFeatures = m_PhysicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceAddressBindingReportFeaturesEXT>();
				addressBindingReportEnabled = addressBindingFeatures.get<vk::PhysicalDeviceAddressBindingReportFeaturesEXT>().reportAddressBinding;
			}

			const bool presentTimingEnabled = VulkanPresentTiming::ResolveSupport(m_PhysicalDevice, m_Surface);
			const bool presentWaitEnabled = VulkanPresentTiming::IsPresentQueueThrottleSupported();

			vk::StructureChain<vk::PhysicalDeviceFeatures2,
			                   vk::PhysicalDeviceVulkan13Features,
			                   vk::PhysicalDeviceVulkan12Features,
			                   vk::PhysicalDeviceVulkan11Features,
			                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
			                   vk::PhysicalDeviceMemoryPriorityFeaturesEXT,
			                   vk::PhysicalDeviceShaderObjectFeaturesEXT,
			                   vk::PhysicalDeviceDescriptorBufferFeaturesEXT,
			                   vk::PhysicalDeviceFaultFeaturesEXT,
			                   vk::PhysicalDeviceAddressBindingReportFeaturesEXT,
			                   vk::PhysicalDevicePresentTimingFeaturesEXT,
			                   vk::PhysicalDevicePresentId2FeaturesKHR,
			                   vk::PhysicalDevicePresentWait2FeaturesKHR,
								vk::PhysicalDeviceRobustness2FeaturesKHR> featureChain = {
					{
						.features = {
							.robustBufferAccess = true,
							.multiDrawIndirect = true,
							.fillModeNonSolid = true,
							.wideLines = true,
							.samplerAnisotropy = true,
							.shaderInt64 = true
						}
					},
					{
						.synchronization2 = true,
						.dynamicRendering = true
					},
					{
						.drawIndirectCount = true,
						.descriptorIndexing = true,
						.shaderSampledImageArrayNonUniformIndexing = true,
						.shaderStorageBufferArrayNonUniformIndexing = true,
						.shaderStorageImageArrayNonUniformIndexing = true,
						.descriptorBindingSampledImageUpdateAfterBind = true,
						.descriptorBindingStorageImageUpdateAfterBind = true,
						.descriptorBindingStorageBufferUpdateAfterBind = true,
						.descriptorBindingPartiallyBound = true,
						.runtimeDescriptorArray = true,
						.samplerFilterMinmax = true,
						.scalarBlockLayout = true,

						.hostQueryReset = true,
						.timelineSemaphore = true,
						.bufferDeviceAddress = true
					},
					{.shaderDrawParameters = true},
					{.extendedDynamicState = true},
					{.memoryPriority = true},
					{.shaderObject = true},
					{.descriptorBuffer = true},
					{ .deviceFault = deviceFaultEnabled },
					{ .reportAddressBinding = addressBindingReportEnabled },
					{ .presentTiming = presentTimingEnabled },
					{ .presentId2 = presentTimingEnabled },
					{ .presentWait2 = presentWaitEnabled },
					{
						.robustBufferAccess2 = true,
						.robustImageAccess2 = true,
						.nullDescriptor = true
					}
				};

			if (!deviceFaultEnabled) {
				featureChain.unlink<vk::PhysicalDeviceFaultFeaturesEXT>();
			}

			if (!addressBindingReportEnabled) {
				featureChain.unlink<vk::PhysicalDeviceAddressBindingReportFeaturesEXT>();
			}

			if (!presentTimingEnabled) {
				featureChain.unlink<vk::PhysicalDevicePresentTimingFeaturesEXT>();
				featureChain.unlink<vk::PhysicalDevicePresentId2FeaturesKHR>();
			}

			if (!presentWaitEnabled) {
				featureChain.unlink<vk::PhysicalDevicePresentWait2FeaturesKHR>();
			}

			float graphicsAndPresentQueuePriority = 1.0f;
			float transferQueuePriority = 0.7f;
			std::vector<vk::DeviceQueueCreateInfo> graphicsAndPresentQueueFamilies;
			if (m_DedicatedPresentQueue) {
				vk::DeviceQueueCreateInfo graphicsDeviceQueueCreateInfo {
					.queueFamilyIndex = m_GraphicsQueueFamilyIndex,
					.queueCount = 1,
					.pQueuePriorities = &graphicsAndPresentQueuePriority
				};

				vk::DeviceQueueCreateInfo presentDeviceQueueCreateInfo {
					.queueFamilyIndex = m_PresentQueueFamilyIndex,
					.queueCount = 1,
					.pQueuePriorities = &graphicsAndPresentQueuePriority
				};

				graphicsAndPresentQueueFamilies.emplace_back(graphicsDeviceQueueCreateInfo);
				graphicsAndPresentQueueFamilies.emplace_back(presentDeviceQueueCreateInfo);
			} else {
				vk::DeviceQueueCreateInfo graphicsDeviceQueueCreateInfo {
					.queueFamilyIndex = m_GraphicsQueueFamilyIndex,
					.queueCount = 1,
					.pQueuePriorities = &graphicsAndPresentQueuePriority
				};

				graphicsAndPresentQueueFamilies.emplace_back(graphicsDeviceQueueCreateInfo);
			}

			if (m_TransferQueueFamilyIndex != m_GraphicsQueueFamilyIndex) {
				vk::DeviceQueueCreateInfo transferDeviceQueueCreateInfo {
					.queueFamilyIndex = m_TransferQueueFamilyIndex,
					.queueCount = 1,
					.pQueuePriorities = &transferQueuePriority
				};

				graphicsAndPresentQueueFamilies.emplace_back(transferDeviceQueueCreateInfo);
				m_DedicatedTransferQueue = true;
			} else {
				if (queueFamilyProperties[m_GraphicsQueueFamilyIndex].queueCount > 1) {
					graphicsAndPresentQueueFamilies[0].queueCount = 2;
					m_DedicatedTransferQueue = true;
				}
			}

			m_NvidiaInstrumentation = m_NvidiaInstrumentation
				&& IsDeviceExtensionEnabled(vk::NVDeviceDiagnosticCheckpointsExtensionName)
				&& IsDeviceExtensionEnabled(vk::NVDeviceDiagnosticsConfigExtensionName);

			vk::PhysicalDeviceDiagnosticsConfigFeaturesNV nvidiaDiagnosticsFeatures{
				.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
				.diagnosticsConfig = vk::True
			};

			vk::DeviceDiagnosticsConfigCreateInfoNV nvidiaDiagnosticsConfig{
				.pNext = &nvidiaDiagnosticsFeatures,
				.flags = vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking
					| vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints
					| vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo
					| vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderErrorReporting
			};

			vk::DeviceCreateInfo deviceCreateInfo{
				.pNext = m_NvidiaInstrumentation ? static_cast<const void*>(&nvidiaDiagnosticsConfig) : static_cast<const void*>(&featureChain.get<vk::PhysicalDeviceFeatures2>()),
				.queueCreateInfoCount = static_cast<uint32_t>(graphicsAndPresentQueueFamilies.size()),
				.pQueueCreateInfos = graphicsAndPresentQueueFamilies.data(),
				.enabledExtensionCount = static_cast<uint32_t>(m_EnabledDeviceExtensions.size()),
				.ppEnabledExtensionNames = m_EnabledDeviceExtensions.data()
			};

			VkDevice device;
			result = static_cast<vk::Result>(vkCreateDevice(&*m_PhysicalDevice, &*deviceCreateInfo, nullptr, &device));
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create logical device. Error: {}", vk::to_string(result));
			m_Device = vk::Device(device);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);

			VulkanDeviceLossDebug::SetAmdSupport(deviceFaultEnabled, addressBindingReportEnabled, IsDeviceExtensionEnabled(vk::AMDBufferMarkerExtensionName));
			VulkanDeviceLossDebug::SetNvidiaInstrumented(m_NvidiaInstrumentation);
			VulkanPresentTiming::OnDeviceCreated(m_Device);

			m_Device.getQueue(m_GraphicsQueueFamilyIndex, 0, &m_GraphicsQueue);
			SetDebugName(m_GraphicsQueue, "Graphics Queue");
			m_Device.getQueue(m_PresentQueueFamilyIndex, 0, &m_PresentQueue);
			if (m_PresentQueueFamilyIndex != m_GraphicsQueueFamilyIndex) {
				SetDebugName(m_PresentQueue, "Present Queue");
			}

			if (m_DedicatedTransferQueue) {
				if (m_TransferQueueFamilyIndex != m_GraphicsQueueFamilyIndex) {
					m_Device.getQueue(m_TransferQueueFamilyIndex, 0, &m_TransferQueue);
					SetDebugName(m_TransferQueue, "Transfer Queue");
				} else {
					if (queueFamilyProperties[m_GraphicsQueueFamilyIndex].queueCount > 1) {
						m_Device.getQueue(m_TransferQueueFamilyIndex, 1, &m_TransferQueue);
						SetDebugName(m_TransferQueue, "Transfer Queue");
					} else {
						m_Device.getQueue(m_TransferQueueFamilyIndex, 0, &m_PresentQueue);
					}
				}
			}

			m_ComputeQueueFamilyIndex = m_GraphicsQueueFamilyIndex;

			constexpr QueueUsageFlags g = QueueUsageFlagBits::eGraphics;
			constexpr QueueUsageFlags t = QueueUsageFlagBits::eTransfer;
			constexpr QueueUsageFlags c = QueueUsageFlagBits::eCompute;

			constexpr QueueUsageFlags gt = QueueUsageFlagBits::eGraphics | QueueUsageFlagBits::eTransfer;
			constexpr QueueUsageFlags tc = QueueUsageFlagBits::eTransfer | QueueUsageFlagBits::eCompute;
			constexpr QueueUsageFlags cg = QueueUsageFlagBits::eCompute | QueueUsageFlagBits::eGraphics;

			constexpr QueueUsageFlags gtc = QueueUsageFlagBits::eGraphics | QueueUsageFlagBits::eTransfer | QueueUsageFlagBits::eCompute;

			m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(g)] = std::make_pair(vk::SharingMode::eExclusive, std::vector<uint32_t>{} );
			m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(t)] = std::make_pair(vk::SharingMode::eExclusive, std::vector<uint32_t>{} );
			m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(c)] = std::make_pair(vk::SharingMode::eExclusive, std::vector<uint32_t>{} );

			if (m_GraphicsQueueFamilyIndex == m_TransferQueueFamilyIndex) {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(gt)] = std::make_pair(vk::SharingMode::eExclusive, std::vector<uint32_t>{} );
			} else {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(gt)] = std::make_pair(vk::SharingMode::eConcurrent, std::vector<uint32_t>{ m_GraphicsQueueFamilyIndex, m_TransferQueueFamilyIndex } );
			}

			if (m_TransferQueueFamilyIndex == m_ComputeQueueFamilyIndex) {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(tc)] = std::make_pair(vk::SharingMode::eExclusive, std::vector<uint32_t>{} );
			} else {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(tc)] = std::make_pair(vk::SharingMode::eConcurrent, std::vector<uint32_t>{ m_TransferQueueFamilyIndex, m_ComputeQueueFamilyIndex } );
			}

			if (m_ComputeQueueFamilyIndex == m_GraphicsQueueFamilyIndex) {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(cg)] = std::make_pair(vk::SharingMode::eExclusive, std::vector<uint32_t>{} );
			} else {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(cg)] = std::make_pair(vk::SharingMode::eConcurrent, std::vector<uint32_t>{ m_ComputeQueueFamilyIndex, m_GraphicsQueueFamilyIndex } );
			}

			if (m_GraphicsQueueFamilyIndex == m_TransferQueueFamilyIndex && m_TransferQueueFamilyIndex == m_ComputeQueueFamilyIndex) {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(gtc)] = std::make_pair(vk::SharingMode::eExclusive, std::vector<uint32_t>{} );
			} else if ((m_GraphicsQueueFamilyIndex == m_ComputeQueueFamilyIndex && m_TransferQueueFamilyIndex != m_ComputeQueueFamilyIndex) || (m_GraphicsQueueFamilyIndex != m_ComputeQueueFamilyIndex && m_TransferQueueFamilyIndex == m_ComputeQueueFamilyIndex)) {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(gtc)] = std::make_pair(vk::SharingMode::eConcurrent, std::vector<uint32_t>{ m_GraphicsQueueFamilyIndex, m_TransferQueueFamilyIndex } );
			} else {
				m_SharingSettings[static_cast<QueueUsageFlags::MaskType>(gtc)] = std::make_pair(vk::SharingMode::eConcurrent, std::vector<uint32_t>{ m_GraphicsQueueFamilyIndex, m_TransferQueueFamilyIndex, m_ComputeQueueFamilyIndex } );
			}
		}

		void VulkanEngine::CreateGPUProfilerContexts() {
			#ifdef CORI_ENABLE_PROFILING
			#if defined(_WIN32)
			constexpr vk::TimeDomainEXT requiredTimeDomain = vk::TimeDomainEXT::eQueryPerformanceCounter;
			#else
			constexpr vk::TimeDomainEXT requiredTimeDomain = vk::TimeDomainEXT::eClockMonotonicRaw;
			#endif

			if (m_PhysicalDeviceProperties.limits.timestampPeriod == 0.0f) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Physical device reports a timestampPeriod of 0, GPU timestamps are unsupported. Tracy GPU zones will be disabled.");
				return;
			}

			auto [domainsResult, timeDomains] = m_PhysicalDevice.getCalibrateableTimeDomainsEXT();
			if (domainsResult != vk::Result::eSuccess) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to query calibrateable time domains ({}). Tracy GPU zones will be disabled.", vk::to_string(domainsResult));
				return;
			}

			if (std::ranges::find(timeDomains, requiredTimeDomain) == timeDomains.end()) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Device does not expose the '{}' calibrateable time domain that Tracy requires to put GPU timestamps on the CPU clock. Tracy GPU zones will be disabled.", vk::to_string(requiredTimeDomain));
				return;
			}

			const auto queueFamilyProperties = m_PhysicalDevice.getQueueFamilyProperties();

			auto queueFamilySupportsTimestamps = [&queueFamilyProperties](const uint32_t familyIndex) {
				return familyIndex < queueFamilyProperties.size() && queueFamilyProperties[familyIndex].timestampValidBits > 0;
			};

			if (queueFamilySupportsTimestamps(m_GraphicsQueueFamilyIndex)) {
				m_GraphicsGPUProfilerContext = CORI_PROFILE_GPU_CONTEXT(m_PhysicalDevice, m_Device,
					VULKAN_HPP_DEFAULT_DISPATCHER.vkResetQueryPool,
					VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
					VULKAN_HPP_DEFAULT_DISPATCHER.vkGetCalibratedTimestampsEXT);
				CORI_PROFILE_GPU_CONTEXT_NAME(m_GraphicsGPUProfilerContext, "Graphics Queue");
			} else {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Graphics queue family {} reports 0 valid timestamp bits, its Tracy GPU context will be disabled.", m_GraphicsQueueFamilyIndex);
			}

			if (m_DedicatedTransferQueue) {
				if (queueFamilySupportsTimestamps(m_TransferQueueFamilyIndex)) {
					m_TransferGPUProfilerContext = CORI_PROFILE_GPU_CONTEXT(m_PhysicalDevice, m_Device,
						VULKAN_HPP_DEFAULT_DISPATCHER.vkResetQueryPool,
						VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
						VULKAN_HPP_DEFAULT_DISPATCHER.vkGetCalibratedTimestampsEXT);
					CORI_PROFILE_GPU_CONTEXT_NAME(m_TransferGPUProfilerContext, "Transfer Queue");
				} else {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Transfer queue family {} reports 0 valid timestamp bits, its Tracy GPU context will be disabled.", m_TransferQueueFamilyIndex);
				}
			} else {
				m_TransferGPUProfilerContext = m_GraphicsGPUProfilerContext;
			}
			#endif
		}

		void VulkanEngine::InitializeVMA() {
			vma::AllocatorCreateInfo info{
				.flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress | vma::AllocatorCreateFlagBits::eExtMemoryBudget | vma::AllocatorCreateFlagBits::eExtMemoryPriority,
				.physicalDevice = m_PhysicalDevice,
				.device = m_Device,
				.instance = m_Instance,
				.vulkanApiVersion = VK_API_VERSION_1_3
			};

			auto [result, alloc] = vma::createAllocator(info);

			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to initialize vma. Error: {}", vk::to_string(result));
			m_Allocator = alloc;
		}

		vk::Extent2D VulkanEngine::GetTargetSwapChainExtent() {
			vk::Extent2D unclamped;
			auto caps = m_PhysicalDevice.getSurfaceCapabilitiesKHR(m_Surface);
			if (caps->currentExtent.height != 0xFFFFFFFF && caps->currentExtent.width != 0xFFFFFFFF) {
				unclamped = caps->currentExtent;
			} else {
				unclamped = vk::Extent2D{ m_NewWindowSizeX.load(std::memory_order_relaxed), m_NewWindowSizeY.load(std::memory_order_relaxed) };
			}

			return {
				std::clamp<uint32_t>(unclamped.width, caps->minImageExtent.width, caps->maxImageExtent.width),
				std::clamp<uint32_t>(unclamped.height, caps->minImageExtent.height, caps->maxImageExtent.height)
			};
		}

		void VulkanEngine::CreateSwapChain() {
			auto [result, surfaceCapabilities] = m_PhysicalDevice.getSurfaceCapabilitiesKHR(m_Surface);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to get surface capabilities. Error: {}", vk::to_string(result));

			auto [result_, availableFormats] = m_PhysicalDevice.getSurfaceFormatsKHR(m_Surface);
			CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to get available surface formats. Error: {}", vk::to_string(result_));

			auto [result__, availablePresentModes] = m_PhysicalDevice.getSurfacePresentModesKHR(m_Surface);
			CORI_CORE_ASSERT(result__ == vk::Result::eSuccess, "Failed to get available surface present modes. Error: {}", vk::to_string(result__));


			auto ChooseFormat = [&] {
				for (const auto& availableFormat : availableFormats) {
					if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
						return availableFormat;
					}
				}

				return availableFormats[0];
			};

			auto ChooseMode = [&]{
				for (const auto& availablePresentMode : availablePresentModes) {
					if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
						return availablePresentMode;
					}
				}

				return vk::PresentModeKHR::eFifo;
			};

			m_SwapChainImageFormat = ChooseFormat();
			auto mode = ChooseMode();
			m_SwapChainExtent = GetTargetSwapChainExtent();
			auto minImageCount = std::max( 3u, surfaceCapabilities.minImageCount );
			if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
				minImageCount = surfaceCapabilities.maxImageCount;
			}

			vk::SwapchainCreateInfoKHR swapChainCreateInfo{
				.flags = VulkanPresentTiming::GetSwapChainCreateFlags(),
				.surface = m_Surface,
				.minImageCount = minImageCount,
				.imageFormat = m_SwapChainImageFormat.format,
				.imageColorSpace = m_SwapChainImageFormat.colorSpace,
				.imageExtent = m_SwapChainExtent,
				.imageArrayLayers =1,
				.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
				.imageSharingMode = vk::SharingMode::eExclusive,
				.preTransform = surfaceCapabilities.currentTransform,
				.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
				.presentMode = mode,
				.clipped = vk::True,
				.oldSwapchain = nullptr };

			if (m_GraphicsQueueFamilyIndex != m_PresentQueueFamilyIndex) {
				uint32_t queueFamilyIndices[] = {m_GraphicsQueueFamilyIndex, m_PresentQueueFamilyIndex};
				swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
				swapChainCreateInfo.queueFamilyIndexCount = 2;
				swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
			}

			result = m_Device.createSwapchainKHR(&swapChainCreateInfo, nullptr, &m_SwapChain);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create swapchain. Error: {}", vk::to_string(result));
			auto [result____, images] = m_Device.getSwapchainImagesKHR(m_SwapChain);
			CORI_CORE_ASSERT(result____ == vk::Result::eSuccess, "Failed to get swapchain images. Error: {}", vk::to_string(result____));
			m_SwapChainImages = std::move(images);

			m_NewWindowSizeX.store(m_SwapChainExtent.width, std::memory_order_relaxed);
			m_NewWindowSizeY.store(m_SwapChainExtent.height, std::memory_order_relaxed);

			SetDebugName(m_SwapChain, "Main SwapChain");

			VulkanPresentTiming::OnSwapChainCreated(m_SwapChain, static_cast<uint32_t>(m_SwapChainImages.size()));

			vk::ImageViewCreateInfo imageViewCreateInfo{
				.viewType = vk::ImageViewType::e2D,
				.format = m_SwapChainImageFormat.format,
				.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
			};

			uint32_t count = 0;

			for (auto image : m_SwapChainImages) {
				imageViewCreateInfo.image = image;
				vk::ImageView view;
				result = m_Device.createImageView(&imageViewCreateInfo, nullptr, &view);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create swapchain image view. Error: {}", vk::to_string(result));
				m_SwapChainImageViews.emplace_back(view);

				std::string name = std::format("SwapChain image view {}", count);

				std::string name_ = std::format("SwapChain image {}", count);

				SetDebugName(image, name_);

				SetDebugName(view, name);

				count++;
			}
		}

		void VulkanEngine::ResizeSwapChain() {
			auto result = m_Device.waitIdle();
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Calling wait idle on device has failed. Error: {}", vk::to_string(result));

			VulkanPresentTiming::OnSwapChainDestroyed();

			for (auto& imageView : m_SwapChainImageViews) {
				m_Device.destroyImageView(imageView);
			}

			m_SwapChainImageViews.clear();
			m_Device.destroySwapchainKHR(m_SwapChain);
			CreateSwapChain();
		}

		void VulkanEngine::CreateCommandPools() {
			vk::CommandPoolCreateInfo poolInfo{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = m_GraphicsQueueFamilyIndex
			};

			auto [result, pool] = m_Device.createCommandPool(poolInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create graphics command pool. Error: {}", vk::to_string(result));
			m_GraphicsCommandPool = pool;
			SetDebugName(m_GraphicsCommandPool, "VulkanEngine graphics command pool");

			vk::CommandPoolCreateInfo poolInfo_{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = m_TransferQueueFamilyIndex
			};

			auto [result_, pool_] = m_Device.createCommandPool(poolInfo_);
			CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create transfer command pool. Error: {}", vk::to_string(result_));
			m_TransferCommandPool = pool_;
			SetDebugName(m_TransferCommandPool, "VulkanEngine transfer command pool");
		}

		void VulkanEngine::CreateCommandBuffer() {
			vk::CommandBufferAllocateInfo allocInfo{
				.commandPool = m_GraphicsCommandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = FRAMES_IN_FLIGHT
			};

			auto [result, buffer] = m_Device.allocateCommandBuffers(allocInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create command buffers. Error: {}", vk::to_string(result));

			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
				m_FrameData[i].m_CommandBuffer = buffer[i];
				std::string name = std::format("VulkanEngine main command buffer {}", i);
				SetDebugName(m_FrameData[i].m_CommandBuffer, name);
			}
		}

		void VulkanEngine::CreateSyncObjects() {
			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
				auto [result, semaphore] = m_Device.createSemaphore(vk::SemaphoreCreateInfo());
				auto [result_, fence] = m_Device.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create semaphore. Error: {}", vk::to_string(result));
				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create fence. Error: {}", vk::to_string(result));
				m_FrameData[i].m_PresentCompleteSemaphore = semaphore;
				m_FrameData[i].m_DrawFence = fence;

				std::string name = std::format("PresentCompleteSemaphore {}", i);
				SetDebugName(m_FrameData[i].m_PresentCompleteSemaphore, name);
				name = std::format("DrawFence {}", i);
				SetDebugName(m_FrameData[i].m_DrawFence, name);
			}

			for (size_t i = 0; i < m_SwapChainImages.size(); i++) {
				auto [result, semaphore] = m_Device.createSemaphore(vk::SemaphoreCreateInfo());
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create semaphore. Error: {}", vk::to_string(result));
				m_RenderFinishedSemaphores.emplace_back(semaphore);
				std::string name = std::format("RenderFinishedSemaphore {}", i);
				SetDebugName(semaphore, name);
			}
		}
	}
}
