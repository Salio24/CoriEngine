#include "VulkanBufferMarkers.hpp"

#ifdef CORI_VK_DL_DEBUG_AMD

#include "Graphics/Vulkan/VulkanEngine.hpp"
#include "Graphics/Vulkan/VulkanBuffer.hpp"

#include <cstring>

namespace Cori {
	namespace Graphics {
		bool VulkanBufferMarkers::s_Supported{ false };
		VulkanBuffer VulkanBufferMarkers::s_Buffer;
		uint32_t* VulkanBufferMarkers::s_Mapped{ nullptr };
		uint32_t VulkanBufferMarkers::s_CurrentRegion{ 0 };
		uint32_t VulkanBufferMarkers::s_Generation{ 0 };
		std::array<VulkanBufferMarkers::FrameRegion, VulkanBufferMarkers::s_HistoryDepth> VulkanBufferMarkers::s_Regions{};

		void VulkanBufferMarkers::SetSupport(const bool supported) {
			s_Supported = supported;
		}

		bool VulkanBufferMarkers::IsSupported() {
			return s_Supported;
		}

		void VulkanBufferMarkers::Init() {
			if (!s_Supported) {
				return;
			}

			const auto& [sharingMode, queueFamilies] = VulkanEngine::GetBufferSharingSettings(QueueUsageFlagBits::eGraphics);

			vk::BufferCreateInfo bufferCreateInfo{
				.size = sizeof(uint32_t) * s_MarkersPerFrame * s_HistoryDepth,
				.usage = vk::BufferUsageFlagBits::eTransferDst,
				.sharingMode = sharingMode,
				.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
				.pQueueFamilyIndices = queueFamilies.data()
			};

			vma::AllocationCreateInfo allocationCreateInfo{
				.flags = vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eMapped,
				.usage = vma::MemoryUsage::eAuto,
				.requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				.preferredFlags = vk::MemoryPropertyFlagBits::eHostCached
			};

			VulkanBuffer::CreateInfo createInfo{
				.bufferCreateInfo = &bufferCreateInfo,
				.allocationCreateInfo = &allocationCreateInfo,
				.name = "GPU breadcrumb marker buffer"
			};

			s_Buffer = VulkanBuffer::Create(createInfo);
			s_Mapped = static_cast<uint32_t*>(VulkanEngine::GetAllocator().getAllocationInfo(s_Buffer.m_Allocation).pMappedData);

			if (!s_Mapped) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "GPU breadcrumb marker buffer could not be persistently mapped, breadcrumbs are disabled.");
				s_Buffer.Destroy();
				s_Supported = false;
				return;
			}

			std::memset(s_Mapped, 0, sizeof(uint32_t) * s_MarkersPerFrame * s_HistoryDepth);

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "GPU breadcrumbs armed via VK_AMD_buffer_marker, {} markers across {} frames of history.", s_MarkersPerFrame, s_HistoryDepth);
		}

		void VulkanBufferMarkers::Shutdown() {
			if (!s_Supported) {
				return;
			}

			s_Buffer.Destroy();
			s_Mapped = nullptr;
			s_Supported = false;
		}

		void VulkanBufferMarkers::BeginFrame(const uint64_t frameIndex) {
			if (!s_Supported) {
				return;
			}

			s_CurrentRegion = (s_CurrentRegion + 1) % s_HistoryDepth;
			s_Generation++;

			FrameRegion& region = s_Regions[s_CurrentRegion];
			region.cursor = 0;
			region.generation = s_Generation;
			region.frameIndex = frameIndex;
		}

		void VulkanBufferMarkers::Write(const vk::CommandBuffer cmb, const vk::PipelineStageFlagBits stage, const char* name, const bool isBegin) {
			if (!s_Supported) {
				return;
			}

			FrameRegion& region = s_Regions[s_CurrentRegion];

			if (region.cursor >= s_MarkersPerFrame) {
				return;
			}

			const uint32_t slotIndex = region.cursor;
			region.cursor++;

			MarkerSlot& slot = region.slots[slotIndex];
			slot.isBegin = isBegin;

			const std::string_view source{ name ? name : "" };
			const uint64_t copyLength = std::min<uint64_t>(source.size(), s_MaxNameLength - 1);
			std::memcpy(slot.name.data(), source.data(), copyLength);
			slot.name[copyLength] = '\0';

			const uint64_t offset = sizeof(uint32_t) * (s_CurrentRegion * s_MarkersPerFrame + slotIndex);
			cmb.writeBufferMarkerAMD(stage, s_Buffer.m_Buffer, offset, region.generation);
		}

		void VulkanBufferMarkers::Dump(const std::string_view context) {
			if (!s_Supported || !s_Mapped) {
				return;
			}

			for (uint32_t age = 0; age < s_HistoryDepth; age++) {
				const uint32_t regionIndex = (s_CurrentRegion + s_HistoryDepth - age) % s_HistoryDepth;
				const FrameRegion& region = s_Regions[regionIndex];

				if (region.cursor == 0 || region.generation == 0) {
					continue;
				}

				const uint32_t* values = s_Mapped + regionIndex * s_MarkersPerFrame;

				uint32_t reached = 0;
				for (uint32_t i = 0; i < region.cursor; i++) {
					if (values[i] == region.generation) {
						reached++;
					}
				}

				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "GPU breadcrumbs for '{}', frame {} ({} frame(s) back): {} of {} marker(s) reached the GPU.", context, region.frameIndex, age, reached, region.cursor);

				if (reached == region.cursor) {
					continue;
				}

				bool stopReported = false;
				for (uint32_t i = 0; i < region.cursor; i++) {
					const bool hit = values[i] == region.generation;
					const MarkerSlot& slot = region.slots[i];

					if (hit) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\t[{:3}] {} '{}' reached", i, slot.isBegin ? "BEGIN" : "END  ", slot.name.data());
						continue;
					}

					if (!stopReported) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\t[{:3}] {} '{}' NOT reached, the GPU stopped here", i, slot.isBegin ? "BEGIN" : "END  ", slot.name.data());
						stopReported = true;
						continue;
					}

					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\t[{:3}] {} '{}' NOT reached", i, slot.isBegin ? "BEGIN" : "END  ", slot.name.data());
				}
			}
		}
	}
}

#endif
