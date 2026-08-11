#pragma once
#include "Graphics/Vulkan/VulkanHeaders.hpp"
namespace Cori {
	namespace Graphics {
		class AftermathCrashTracker {
		public:
			static constexpr uint32_t s_MarkerFrameHistory{ 4 };

			static void Init();

			static void Shutdown();

			[[nodiscard]] static bool IsActive();

			[[nodiscard]] static bool IsDeviceInstrumented();

			[[nodiscard]] static bool IsSupportedByDevice(vk::PhysicalDevice physicalDevice);

			static void SetDeviceInstrumented(bool instrumented);

			static void AddDumpDescription(std::string description);

			static void RegisterShaderBinary(const void* spirvCode, size_t codeSizeBytes, std::string_view debugName = {});

			static void BeginFrame(uint64_t frameIndex);

			static void SetCheckpoint(vk::CommandBuffer cmb, std::string_view markerData);

			[[nodiscard]] static bool ResolveMarker(uint64_t markerID, std::string& outMarker);

			static void LogQueueCheckpoints(vk::Queue queue, std::string_view queueName);

			static bool OnDeviceLost(std::string_view context);
		};
	}
}

#ifdef CORI_VK_DL_DEBUG_NVIDIA
	#define CORI_AFTERMATH_MARKER(cmb, marker) ::Cori::Graphics::AftermathCrashTracker::SetCheckpoint(cmb, marker)
#else
	#define CORI_AFTERMATH_MARKER(cmb, marker)
#endif
