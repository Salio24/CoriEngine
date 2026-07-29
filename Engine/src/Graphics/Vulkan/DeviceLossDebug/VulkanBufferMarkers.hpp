#pragma once

#ifdef CORI_VK_DL_DEBUG_AMD

#include "Graphics/Vulkan/VulkanBuffer.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanBufferMarkers {
		public:
			static void Init();

			static void Shutdown();

			static void SetSupport(bool supported);

			[[nodiscard]] static bool IsSupported();

			static void BeginFrame(uint64_t frameIndex);

			static void Write(vk::CommandBuffer cmb, vk::PipelineStageFlagBits stage, const char* name, bool isBegin);

			static void Dump(std::string_view context);

		private:
			static constexpr uint32_t s_MarkersPerFrame = 256;
			static constexpr uint32_t s_MaxNameLength = 48;
			static constexpr uint32_t s_HistoryDepth = 8;

			struct MarkerSlot {
				std::array<char, s_MaxNameLength> name{};
				bool isBegin{ false };
			};

			struct FrameRegion {
				std::array<MarkerSlot, s_MarkersPerFrame> slots{};
				uint32_t cursor{ 0 };
				uint32_t generation{ 0 };
				uint64_t frameIndex{ 0 };
			};

			static bool s_Supported;
			static VulkanBuffer s_Buffer;
			static uint32_t* s_Mapped;
			static uint32_t s_CurrentRegion;
			static uint32_t s_Generation;
			static std::array<FrameRegion, s_HistoryDepth> s_Regions;
		};

	}
}

#endif
