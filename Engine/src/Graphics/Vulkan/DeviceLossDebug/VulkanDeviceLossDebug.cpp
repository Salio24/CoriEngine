#include "VulkanDeviceLossDebug.hpp"
#include "Graphics/Vulkan/VulkanEngine.hpp"

#ifdef CORI_VK_DL_DEBUG_AMD
#include "VulkanDeviceFault.hpp"
#include "VulkanBufferMarkers.hpp"
#endif

namespace Cori {
	namespace Graphics {
		void VulkanDeviceLossDebug::Init() {
			AftermathCrashTracker::Init();
		}

		void VulkanDeviceLossDebug::InitDeviceResources() {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanBufferMarkers::Init();
			#endif
		}

		void VulkanDeviceLossDebug::Shutdown() {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanBufferMarkers::Shutdown();
			#endif

			AftermathCrashTracker::Shutdown();
		}

		bool VulkanDeviceLossDebug::IsNvidiaInstrumentationSupported(const vk::PhysicalDevice physicalDevice) {
			return AftermathCrashTracker::IsSupportedByDevice(physicalDevice);
		}

		void VulkanDeviceLossDebug::SetAmdSupport([[maybe_unused]] const bool deviceFault, [[maybe_unused]] const bool addressBinding, [[maybe_unused]] const bool bufferMarkers) {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanDeviceFault::SetSupport(deviceFault);
			VulkanDeviceFault::SetAddressBindingTracking(addressBinding);
			VulkanBufferMarkers::SetSupport(bufferMarkers);
			#endif
		}

		void VulkanDeviceLossDebug::SetNvidiaInstrumented(const bool instrumented) {
			AftermathCrashTracker::SetDeviceInstrumented(instrumented);
		}

		void VulkanDeviceLossDebug::AddDumpDescription(std::string description) {
			AftermathCrashTracker::AddDumpDescription(std::move(description));
		}

		void VulkanDeviceLossDebug::RegisterShaderBinary(const void* spirvCode, const size_t codeSizeBytes, const std::string_view debugName) {
			AftermathCrashTracker::RegisterShaderBinary(spirvCode, codeSizeBytes, debugName);
		}

		void VulkanDeviceLossDebug::BeginFrame(const uint64_t frameIndex) {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanBufferMarkers::BeginFrame(frameIndex);
			#endif

			AftermathCrashTracker::BeginFrame(frameIndex);
		}

		void VulkanDeviceLossDebug::RecordObjectName([[maybe_unused]] const uint64_t objectHandle, [[maybe_unused]] const char* name) {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanDeviceFault::RecordObjectName(objectHandle, name);
			#endif
		}

		void VulkanDeviceLossDebug::RecordAddressBinding([[maybe_unused]] const uint64_t baseAddress, [[maybe_unused]] const uint64_t size, [[maybe_unused]] const bool unbind, [[maybe_unused]] const bool internalObject, [[maybe_unused]] const vk::ObjectType objectType, [[maybe_unused]] const uint64_t objectHandle) {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanDeviceFault::RecordAddressBinding(baseAddress, size, unbind, internalObject, objectType, objectHandle);
			#endif
		}

		void VulkanDeviceLossDebug::MarkerBegin([[maybe_unused]] const vk::CommandBuffer cmb, [[maybe_unused]] const char* name) {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanBufferMarkers::Write(cmb, vk::PipelineStageFlagBits::eTopOfPipe, name, true);
			#endif

			AftermathCrashTracker::SetCheckpoint(cmb, name);
		}

		void VulkanDeviceLossDebug::MarkerEnd([[maybe_unused]] const vk::CommandBuffer cmb, [[maybe_unused]] const char* name) {
			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanBufferMarkers::Write(cmb, vk::PipelineStageFlagBits::eBottomOfPipe, name, false);
			#endif
		}

		bool VulkanDeviceLossDebug::CheckResult(const vk::Result result, const std::string_view context) {
			if (result != vk::Result::eErrorDeviceLost) {
				return false;
			}

			OnDeviceLost(context);

			return true;
		}

		void VulkanDeviceLossDebug::OnDeviceLost(const std::string_view context) {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "VK_ERROR_DEVICE_LOST reported by '{}'. The GPU has crashed, lord have mercy.", context);

			#ifdef CORI_VK_DL_DEBUG_AMD
			VulkanBufferMarkers::Dump(context);
			VulkanDeviceFault::Report(context);
			#endif

			if (AftermathCrashTracker::IsDeviceInstrumented()) {
				AftermathCrashTracker::LogQueueCheckpoints(VulkanEngine::GetGraphicsQueue(), "graphics");

				const vk::Queue transferQueue = VulkanEngine::GetTransferQueue();
				if (transferQueue && transferQueue != VulkanEngine::GetGraphicsQueue()) {
					AftermathCrashTracker::LogQueueCheckpoints(transferQueue, "transfer");
				}
			}

			AftermathCrashTracker::OnDeviceLost(context);
		}
	}
}
