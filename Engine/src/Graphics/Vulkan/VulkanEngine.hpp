#pragma once
//#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
//#define VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC 1
//#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
//#define VULKAN_HPP_NO_EXCEPTIONS
//#define VULKAN_HPP_ASSERT_ON_RESULT
//#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
//#include <vulkan/vulkan_raii.hpp>
//#include <vulkan/vk_platform.h>
//#include <vulkan/vulkan_to_string.hpp>
//#include <vulkan/vulkan_format_traits.hpp>
//#include "Profiling/GPUProfiler.hpp"
#include "VulkanHeaders.hpp"
#include "vk_mem_alloc.hpp"
#include "VulkanDebugLabels.hpp"
#include "DeviceLossDebug/VulkanDeviceLossDebug.hpp"
#include "VulkanPresentTiming.hpp"

#define CORI_CHECK_VK_RESULT(result) (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR)

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

//FIXME: explicitly disable VK_EXT_debug_utils in release build

namespace Cori {
	namespace Graphics {
		class SceneRenderer;

		using vk::operator&;
		using vk::operator|;
		using vk::operator^;
		using vk::operator~;
		using vk::operator<;
		using vk::operator<=;
		using vk::operator>;
		using vk::operator>=;
		using vk::operator==;
		using vk::operator!=;

		//NOTE: this is applicable only to buffers, as using CONCURENT sharing mode on images is a terrible idea
		enum class QueueUsageFlagBits : VkFlags {
			eGraphics = 0x00000001,
			eTransfer = 0x00000002,
			eCompute = 0x00000004
		};

		using QueueUsageFlags = vk::Flags<QueueUsageFlagBits>;

		class VulkanEngine {
		public:
			struct FrameInfo {
				vk::CommandBuffer m_CommandBuffer;
				vk::Semaphore m_PresentCompleteSemaphore;
				vk::Fence m_DrawFence;
				uint64_t m_FrameIndex;
				uint32_t m_SwapChainImageIndex;
				bool m_SkippedFrame;
			};

			static void RequestDeviceExtension(const char* extension) {
				m_DeviceExtensions.push_back(extension);
			}

			static void RequestOptionalDeviceExtension(const char* extension) {
				m_OptionalDeviceExtensions.push_back(extension);
			}

			static void RequestInstanceExtension(const char* extension) {
				m_InstanceExtensions.push_back(extension);
			}

			[[nodiscard]] static bool IsDeviceExtensionEnabled(const std::string_view extension);

			static void Start(void* window, const vk::Extent2D swapChainExtent);

			static void Stop();

			static VulkanEngine& Get() {
				CORI_CORE_ASSERT(s_Instance, "Calling VulkanEngine::Get but engine was already destroyed or not yet created.")
				return *s_Instance;
			}

			template<typename HandleType>
			static vk::Result SetDebugName(const HandleType& handle, const std::string& name) {
				static_assert(vk::isVulkanHandleType<HandleType>::value, "HandleType must be a Vulkan handle type" );

				vk::DebugUtilsObjectNameInfoEXT info {
					.objectType = handle.objectType,
					.objectHandle = reinterpret_cast<uint64_t>( static_cast<typename HandleType::CType>(handle)),
					.pObjectName = name.c_str()
				};

				VulkanDeviceLossDebug::RecordObjectName(info.objectHandle, info.pObjectName);

				return Get().m_Device.setDebugUtilsObjectNameEXT(info);
			}

			template<typename HandleType>
			static vk::Result SetDebugName(const HandleType& handle, const char* name) {
				#ifdef DEBUG_BUILD
				static_assert(vk::isVulkanHandleType<HandleType>::value, "HandleType must be a Vulkan handle type" );

				vk::DebugUtilsObjectNameInfoEXT info {
					.objectType = handle.objectType,
					.objectHandle = reinterpret_cast<uint64_t>( static_cast<typename HandleType::CType>(handle)),
					.pObjectName = name
				};

				VulkanDeviceLossDebug::RecordObjectName(info.objectHandle, info.pObjectName);

				return Get().m_Device.setDebugUtilsObjectNameEXT(info);
				#else
				return vk::Result::eSuccess;
				#endif
			}

			static void EnterThreadedMode();

			static void ExitThreadedMode();

			static const std::pair<vk::SharingMode, std::vector<uint32_t>>& GetBufferSharingSettings(const QueueUsageFlags usage);

			static vk::raii::Context& GetContext() {
				return Get().m_Context;
			}

			static vk::Instance GetInstance() {
				return Get().m_Instance;
			}

			static vk::Device GetLogicalDevice() {
				return Get().m_Device;
			}

			static vma::Allocator GetAllocator() {
				return Get().m_Allocator;
			}

			static vk::PhysicalDeviceProperties& GetPhysicalDeviceProperties() {
				return Get().m_PhysicalDeviceProperties;
			}

			static vk::PhysicalDevice GetPhysicalDevice() {
				return Get().m_PhysicalDevice;
			}

			static uint32_t GetCurrentFrameInFlight() {
				return Get().m_CurrentFrameInFlight;
			}

			static uint32_t GetPreviousFrameInFlight() {
				return (Get().m_CurrentFrameInFlight + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT;
			}

			static uint32_t GetNextFrameInFlight() {
				return (Get().m_CurrentFrameInFlight + FRAMES_IN_FLIGHT + 1) % FRAMES_IN_FLIGHT;
			}

			static uint64_t GetFrameIndex() {
				return Get().m_CurrentFrameIndex;
			}

			static vk::Queue& GetTransferQueue() {
				return Get().m_TransferQueue;
			}

			static uint32_t GetTransferQueueFamilyIndex() {
				return Get().m_TransferQueueFamilyIndex;
			}

			static uint32_t GetGraphicsQueueFamilyIndex() {
				return Get().m_GraphicsQueueFamilyIndex;
			}

			static vk::Queue GetGraphicsQueue() {
				return Get().m_GraphicsQueue;
			}

			static vk::CommandPool& GetTransferCmp() {
				return Get().m_TransferCommandPool;
			}

			static GPUProfilerContext GetGraphicsGPUProfilerContext() {
				return Get().m_GraphicsGPUProfilerContext;
			}

			static GPUProfilerContext GetTransferGPUProfilerContext() {
				return Get().m_TransferGPUProfilerContext;
			}

			static void AddWaitSemaphore(const vk::Semaphore semaphore, const vk::PipelineStageFlags waitDstStageMask);

			static void AddWaitTimelineSemaphore(const vk::Semaphore semaphore, const uint64_t value, const vk::PipelineStageFlags waitDstStageMask);

			static vk::ImageView GetSwapChainImageView() {
				return Get().m_SwapChainImageViews[Get().m_CurrentSwapChainImageIndex];
			}

			static vk::Image GetSwapChainImage() {
				return Get().m_SwapChainImages[Get().m_CurrentSwapChainImageIndex];
			}

			static vk::Extent2D GetSwapChainExtent() {
				return Get().m_SwapChainExtent;
			}

			static vk::SurfaceFormatKHR GetSwapChaimImageFormat() {
				return Get().m_SwapChainImageFormat;
			}

			static uint32_t GetSwapChainImageCount() {
				return Get().m_SwapChainImages.size();
			}

			static void ReportWindowResize(const glm::ivec2 newSize) {
				Get().m_NewWindowSizeX.store(newSize.x, std::memory_order_release);
				Get().m_NewWindowSizeY.store(newSize.y, std::memory_order_release);
			}

			void CPUFrameStart();

			FrameInfo& GPUFrameBegin();

			void GPUFrameMiddlePointSync();

			void GPUFrameEnd(const FrameLatencyStamps& latencyStamps);

			~VulkanEngine();

			static bool s_VerboseValidationLayerLogging;
			static bool s_EnableDeviceAddressBindingReport;
		private:
			VulkanEngine(void* window, const vk::Extent2D swapChainExtent);

			void CreateInstance();

			void SetupDebugMessenger();

			void CreateSurface();

			void PickPhysicalDevice();

			void CreateDevice();

			void CreateGPUProfilerContexts();

			void InitializeVMA();
			vk::Extent2D GetTargetSwapChainExtent();

			void CreateSwapChain();

			void ResizeSwapChain();

			void CreateCommandPools();

			void CreateCommandBuffer();

			void CreateSyncObjects();

			friend SceneRenderer;

			vk::raii::Context m_Context;
			vk::DebugUtilsMessengerEXT m_DebugMessenger = nullptr;
			vk::Instance m_Instance = nullptr;
			vk::SurfaceKHR m_Surface = nullptr;
			vk::PhysicalDevice m_PhysicalDevice = nullptr;
			vk::PhysicalDeviceProperties m_PhysicalDeviceProperties;
			vk::Device m_Device = nullptr;

			bool m_DedicatedTransferQueue = false;
			bool m_DedicatedPresentQueue = false;
			bool m_DedicatedComputeQueue = false;

			vk::Queue m_TransferQueue = nullptr;
			vk::Queue m_GraphicsQueue = nullptr;
			vk::Queue m_PresentQueue = nullptr;

			vk::SwapchainKHR m_SwapChain = nullptr;
			vma::Allocator m_Allocator = nullptr;

			vk::CommandPool m_GraphicsCommandPool = nullptr;
			vk::CommandPool m_TransferCommandPool = nullptr;

			uint32_t m_CurrentFrameInFlight = 0;
			uint64_t m_CurrentFrameIndex = 0;

			uint32_t m_CurrentSwapChainImageIndex = 0;

			std::vector<vk::Semaphore> m_RenderFinishedSemaphores;
			std::array<FrameInfo, FRAMES_IN_FLIGHT> m_FrameData;

			std::vector<vk::Semaphore> m_WaitSemaphores;
			std::vector<vk::PipelineStageFlags> m_WaitDstStageMasks;
			std::vector<uint64_t> m_TimelineWaitValues;

			bool m_TimelineSemaphoresPresent{ false };

			std::unordered_map<QueueUsageFlags::MaskType, std::pair<vk::SharingMode, std::vector<uint32_t>>> m_SharingSettings;

			std::vector<vk::Image> m_SwapChainImages;
			std::vector<vk::ImageView> m_SwapChainImageViews;
			vk::SurfaceFormatKHR m_SwapChainImageFormat;
			vk::Extent2D m_SwapChainExtent;
			vk::Extent2D m_DrawImageExtent;

			uint32_t m_TransferQueueFamilyIndex;
			uint32_t m_GraphicsQueueFamilyIndex;
			uint32_t m_PresentQueueFamilyIndex;
			uint32_t m_ComputeQueueFamilyIndex;

			GPUProfilerContext m_GraphicsGPUProfilerContext{ nullptr };
			GPUProfilerContext m_TransferGPUProfilerContext{ nullptr };
			GPUProfilerSpanZone m_GPUFrameZone;

			static std::vector<const char*> m_DeviceExtensions;
			static std::vector<const char*> m_OptionalDeviceExtensions;
			static std::vector<const char*> m_EnabledDeviceExtensions;
			static std::vector<const char*> m_InstanceExtensions;

			bool m_NvidiaInstrumentation{ false };

			bool m_ValidationLayers;

			std::atomic<uint32_t> m_NewWindowSizeX{};
			std::atomic<uint32_t> m_NewWindowSizeY{};

			void* m_Window; // sdl window

			static VulkanEngine* s_Instance;
		};
	}
}

namespace vk {
	template<> struct FlagTraits<Cori::Graphics::QueueUsageFlagBits> {
		using WrappedType = VkFlags;
		static VULKAN_HPP_CONST_OR_CONSTEXPR bool isBitmask = true;
		static VULKAN_HPP_CONST_OR_CONSTEXPR Flags<Cori::Graphics::QueueUsageFlagBits> allFlags =
			Cori::Graphics::QueueUsageFlagBits::eGraphics | Cori::Graphics::QueueUsageFlagBits::eTransfer | Cori::Graphics::QueueUsageFlagBits::eCompute;
	};
}