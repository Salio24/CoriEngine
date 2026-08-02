#pragma once

namespace Cori {
	namespace Graphics {
		struct FrameLatencyStamps {
			uint64_t inputTimestampSdl{ 0 };
			uint64_t frameStartHost{ 0 };
		};

		struct PacingHints {
			// Most recent scanout the driver reported, in the host clock. Zero when nothing has been reported yet.
			uint64_t lastScanoutHost{ 0 };

			uint64_t refreshDurationNs{ 0 };

			// P95 of frameStartHost to the present call, the budget a frame needs to make its deadline.
			uint64_t pipelineEstimate{ 0 };

			uint64_t presentToPhotons{ 0 };

			uint64_t missedRefreshes{ 0 };
		};

		class VulkanPresentTiming {
		public:
			static void RequestExtensions();

			[[nodiscard]] static bool ResolveSupport(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);

			[[nodiscard]] static bool IsEnabled();

			[[nodiscard]] static bool IsPresentQueueThrottleSupported();

			static void OnDeviceCreated(vk::Device device);

			[[nodiscard]] static vk::SwapchainCreateFlagsKHR GetSwapChainCreateFlags();

			static void OnSwapChainCreated(vk::SwapchainKHR swapchain, uint32_t imageCount);

			static void OnSwapChainDestroyed();

			static void Shutdown();

			[[nodiscard]] static const void* PreparePresent(const FrameLatencyStamps& stamps);

			static void FinishPresent(vk::Result result);

			static void Poll();

			static void ThrottlePresentQueue();

			static void SetPresentQueueDepth(uint32_t depth);

			[[nodiscard]] static PacingHints GetPacingHints();

			[[nodiscard]] static uint64_t HostNow();
		private:
			static void Calibrate();
		};
	}
}
