#pragma once

#include "AftermathCrashTracker.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanDeviceLossDebug {
		public:
			static void Init();

			static void InitDeviceResources();

			static void Shutdown();

			[[nodiscard]] static bool IsNvidiaInstrumentationSupported(vk::PhysicalDevice physicalDevice);

			static void SetAmdSupport(bool deviceFault, bool addressBinding, bool bufferMarkers);

			static void SetNvidiaInstrumented(bool instrumented);

			static void AddDumpDescription(std::string description);

			static void RegisterShaderBinary(const void* spirvCode, size_t codeSizeBytes, std::string_view debugName = {});

			static void BeginFrame(uint64_t frameIndex);

			static void RecordObjectName(uint64_t objectHandle, const char* name);

			static void RecordAddressBinding(uint64_t baseAddress, uint64_t size, bool unbind, bool internalObject, vk::ObjectType objectType, uint64_t objectHandle);

			static void MarkerBegin(vk::CommandBuffer cmb, const char* name);

			static void MarkerEnd(vk::CommandBuffer cmb, const char* name);

			static bool CheckResult(vk::Result result, std::string_view context);

			static void OnDeviceLost(std::string_view context);
		};

		class VulkanDeviceLossMarkerScope {
		public:
			VulkanDeviceLossMarkerScope(const vk::CommandBuffer cmb, const char* name) : m_CommandBuffer(cmb), m_Name(name) {
				VulkanDeviceLossDebug::MarkerBegin(m_CommandBuffer, m_Name);
			}

			VulkanDeviceLossMarkerScope(const VulkanDeviceLossMarkerScope&) = delete;
			VulkanDeviceLossMarkerScope& operator=(const VulkanDeviceLossMarkerScope&) = delete;
			VulkanDeviceLossMarkerScope(VulkanDeviceLossMarkerScope&&) = delete;
			VulkanDeviceLossMarkerScope& operator=(VulkanDeviceLossMarkerScope&&) = delete;

			~VulkanDeviceLossMarkerScope() {
				VulkanDeviceLossDebug::MarkerEnd(m_CommandBuffer, m_Name);
			}

		private:
			vk::CommandBuffer m_CommandBuffer;
			const char* m_Name;
		};
	}
}

#define CORI_VK_DL_MARKER_CONCAT_IMPL(a, b) a##b
#define CORI_VK_DL_MARKER_CONCAT(a, b) CORI_VK_DL_MARKER_CONCAT_IMPL(a, b)

#define CORI_VK_DL_MARKER_SCOPE(cmb, name) const ::Cori::Graphics::VulkanDeviceLossMarkerScope CORI_VK_DL_MARKER_CONCAT(___cori_vk_dl_marker_, __LINE__)(cmb, name)
