#pragma once
#include "VulkanEngine.hpp"
#include "VulkanImage.hpp"
#include "VulkanBuffer.hpp"

#if 0

namespace Cori {
	namespace Graphics {
		struct ResourceState {
			vk::PipelineStageFlags2 stageMask{ vk::PipelineStageFlagBits2::eNone };
			vk::AccessFlags2 accessMask{ vk::AccessFlagBits2::eNone };
			vk::ImageLayout imageLayout{ vk::ImageLayout::eUndefined };
			uint32_t queueFamilyIndex{ VK_QUEUE_FAMILY_IGNORED };

			bool operator==(const ResourceState& other) const = default;
		};

		struct ImageSubresourceKey {
			uint32_t baseArrayLayer;
			vk::ImageAspectFlags aspectMask;
			uint32_t baseMipLevel;

			bool operator==(const ImageSubresourceKey& other) const = default;

			auto operator<=>(const ImageSubresourceKey& other) const = default;
		};

		using BufferSubresourceKey = uint64_t;

		class VulkanResourceTracker {
		public:
			VulkanResourceTracker() {
				m_ImageBarriersCache.reserve(64);
				m_BufferBarriersCache.reserve(64);
				m_PendingImageAspectFlags.reserve(3);
				m_ImageStates.reserve(128);
				m_BufferStates.reserve(48);
			}

			static VulkanResourceTracker& Get() {
				static VulkanResourceTracker instance;
				return instance;
			}

			static std::optional<std::vector<vk::ImageMemoryBarrier2>*> TransitionImage(VulkanImage& image, const vk::ImageSubresourceRange& subresourceRange, const ResourceState& desiredState) {
				CORI_PROFILE_FUNCTION();
				uint64_t handle = image.GetRawHandle();

				if (subresourceRange.baseMipLevel + subresourceRange.levelCount > image.m_MipLevels || subresourceRange.baseArrayLayer + subresourceRange.layerCount > image.m_ArrayLayers) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ResourceTracker }, "Invalid subresource range was provided when calling TransitionImage for image '{}', handle '{:#x}'. No barriers were generated.", image.m_Name, handle);
					return std::nullopt;
				}

				auto it = Get().m_ImageStates.find(handle);
				if (it == Get().m_ImageStates.end()) {
					return std::nullopt;
				}

				auto& stateMap = it->second;
				Get().m_PendingImageAspectFlags.clear();
				Get().m_ImageBarriersCache.clear();
				vk::ImageAspectFlags flags = image.GetAspectMask();

				if (subresourceRange.aspectMask & vk::ImageAspectFlagBits::eColor & flags) {
					Get().m_PendingImageAspectFlags.emplace_back(vk::ImageAspectFlagBits::eColor);
				}

				if (subresourceRange.aspectMask & vk::ImageAspectFlagBits::eDepth & flags) {
					Get().m_PendingImageAspectFlags.emplace_back(vk::ImageAspectFlagBits::eDepth);
				}

				if (subresourceRange.aspectMask & vk::ImageAspectFlagBits::eStencil & flags) {
					Get().m_PendingImageAspectFlags.emplace_back(vk::ImageAspectFlagBits::eStencil);
				}

				const uint32_t newRangeStartMip = subresourceRange.baseMipLevel;
				const uint32_t newRangeEndMip = subresourceRange.baseMipLevel + subresourceRange.levelCount;

				for (auto aspect : Get().m_PendingImageAspectFlags) {
					for (uint32_t layer = subresourceRange.baseArrayLayer; layer < subresourceRange.baseArrayLayer + subresourceRange.layerCount; ++layer) {

						auto currentIt = stateMap.upper_bound({layer, aspect, newRangeStartMip});
						if (currentIt != stateMap.begin()) {
							--currentIt;
						}

						while (currentIt != stateMap.end() && currentIt->first.baseArrayLayer == layer && currentIt->first.aspectMask == aspect && currentIt->first.baseMipLevel < newRangeEndMip) {
							const ResourceState oldRangeState = currentIt->second;

							auto nextIt = std::next(currentIt);
							bool sectorEnd = nextIt == stateMap.end() || nextIt->first.baseArrayLayer != layer || nextIt->first.aspectMask != aspect;

							const uint32_t oldRangeStartMip = currentIt->first.baseMipLevel;
							uint32_t oldRangeEndMip = sectorEnd ? image.m_MipLevels : nextIt->first.baseMipLevel;

							bool needsBarrier = oldRangeState != desiredState || (oldRangeState.accessMask & s_WriteFlagMask && desiredState.accessMask & s_WriteFlagMask);
							if (needsBarrier) {
								uint32_t overlapStartMip = std::max(newRangeStartMip, oldRangeStartMip);
								uint32_t overlapEndMip = std::min(newRangeEndMip, oldRangeEndMip);

								if (overlapEndMip > overlapStartMip) {
									Get().m_ImageBarriersCache.emplace_back(vk::ImageMemoryBarrier2{
											.srcStageMask = oldRangeState.stageMask,
											.srcAccessMask = oldRangeState.accessMask,
											.dstStageMask = desiredState.stageMask,
											.dstAccessMask = desiredState.accessMask,
											.oldLayout = oldRangeState.imageLayout,
											.newLayout = desiredState.imageLayout,
											.srcQueueFamilyIndex = oldRangeState.queueFamilyIndex,
											.dstQueueFamilyIndex = desiredState.queueFamilyIndex,
											.image = image.m_Image,
											.subresourceRange = {
												.aspectMask = aspect,
												.baseMipLevel = overlapStartMip,
												.levelCount = overlapEndMip - overlapStartMip,
												.baseArrayLayer = layer,
												.layerCount = 1
											}
										}
									);
								}
							}
							currentIt = nextIt;
						}
					}
				}

				for (auto aspect : Get().m_PendingImageAspectFlags) {
					for (uint32_t layer = subresourceRange.baseArrayLayer; layer < subresourceRange.baseArrayLayer + subresourceRange.layerCount; ++layer) {
						ImageSubresourceKey rangeStartMipKey = { layer, aspect, newRangeStartMip };
						ImageSubresourceKey rangeEndMipKey = { layer, aspect, newRangeEndMip };

						auto endIt = stateMap.lower_bound(rangeEndMipKey);
						if (endIt != stateMap.begin()) {
							auto prevIt = std::prev(endIt);

							bool sectorEnd = endIt == stateMap.end() || endIt->first.baseArrayLayer != layer || endIt->first.aspectMask != aspect;

							uint32_t prevRangeEndMip = sectorEnd ? image.m_MipLevels : endIt->first.baseMipLevel;
							if (newRangeEndMip < prevRangeEndMip) {
								if (prevIt->second != desiredState) {
									stateMap[rangeEndMipKey] = prevIt->second;
								}
							}
						}

						stateMap.erase(stateMap.lower_bound(rangeStartMipKey), stateMap.upper_bound(rangeEndMipKey));

						auto [newIt, _] = stateMap.emplace(rangeStartMipKey, desiredState);

						auto nextIt = std::next(newIt);
						if (nextIt != stateMap.end() && nextIt->first.baseArrayLayer == layer && nextIt->first.aspectMask == aspect && nextIt->second == newIt->second) {
							stateMap.erase(nextIt);
						}

						if (newIt != stateMap.begin()) {
							auto prevIt = std::prev(newIt);
							if (prevIt->first.baseArrayLayer == layer && prevIt->first.aspectMask == aspect && prevIt->second == newIt->second) {
								stateMap.erase(newIt);
							}
						}
					}
				}

				if (!Get().m_ImageBarriersCache.empty()) {
					return &Get().m_ImageBarriersCache;
				}

				return std::nullopt;
			}

			static std::optional<std::vector<vk::BufferMemoryBarrier2>*> TransitionBuffer(VulkanBuffer& buffer, const uint64_t offset, const uint64_t size, const ResourceState& desiredState) {
				CORI_PROFILE_FUNCTION();
				uint64_t handle = buffer.GetRawHandle();

				if (offset > buffer.m_Size || (offset + size > buffer.m_Size && size != VK_WHOLE_SIZE)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ResourceTracker }, "Invalid offset or size was provided when calling TransitionBuffer for buffer '{}', handle '{:#x}'. No barriers were generated.", buffer.m_Name, handle);
					return std::nullopt;
				}

				auto it = Get().m_BufferStates.find(handle);
				if (it == Get().m_BufferStates.end()) {
					return std::nullopt;
				}

				auto& stateMap = it->second;
				Get().m_BufferBarriersCache.clear();

				uint64_t newRangeStart = offset;
				uint64_t newRangeEnd = size == VK_WHOLE_SIZE ? buffer.m_Size : offset + size;

				auto currentIt = stateMap.upper_bound(newRangeStart);
				if (currentIt != stateMap.begin()) {
					--currentIt;
				}

				while (currentIt != stateMap.end() && currentIt->first < newRangeEnd) {
					ResourceState oldRangeState = currentIt->second;

					auto nextIt = std::next(currentIt);
					bool sectorEnd = nextIt == stateMap.end();

					uint64_t oldRangeStart = currentIt->first;
					uint64_t oldRangeEnd = sectorEnd ? buffer.m_Size : nextIt->first;

					bool needsBarrier = oldRangeState != desiredState || (oldRangeState.accessMask & s_WriteFlagMask && desiredState.accessMask & s_WriteFlagMask);
					if (needsBarrier) {
						uint64_t overlapStart = std::max(newRangeStart, oldRangeStart);
						uint64_t overlapEnd = std::min(newRangeEnd, oldRangeEnd);

						if (overlapEnd > overlapStart) {
							Get().m_BufferBarriersCache.emplace_back(vk::BufferMemoryBarrier2{
								.srcStageMask = oldRangeState.stageMask,
								.srcAccessMask = oldRangeState.accessMask,
								.dstStageMask = desiredState.stageMask,
								.dstAccessMask = desiredState.accessMask,
								.srcQueueFamilyIndex = oldRangeState.queueFamilyIndex,
								.dstQueueFamilyIndex = desiredState.queueFamilyIndex,
								.buffer = buffer.m_Buffer,
								.offset = overlapStart,
								.size = overlapEnd - overlapStart
							});
						}
					}
					currentIt = nextIt;
				}

				auto endIt = stateMap.lower_bound(newRangeEnd);
				if (endIt != stateMap.begin()) {
					auto prevIt = std::prev(endIt);

					bool sectorEnd = endIt == stateMap.end();

					uint64_t prevRangeEnd = sectorEnd ? buffer.m_Size : endIt->first;
					if (newRangeEnd < prevRangeEnd) {
						if (prevIt->second != desiredState) {
							stateMap[newRangeEnd] = prevIt->second;
						}
					}
				}

				auto fIt = stateMap.lower_bound(newRangeStart);
				auto lIt = stateMap.upper_bound(newRangeEnd);

				stateMap.erase(fIt, lIt);

				auto [newIt, _] = stateMap.emplace(newRangeStart, desiredState);

				auto nextIt = std::next(newIt);
				if (nextIt != stateMap.end() && nextIt->second == newIt->second) {
					stateMap.erase(nextIt);
				}

				if (newIt != stateMap.begin()) {
					auto prevIt = std::prev(newIt);
					if (prevIt->second == newIt->second) {
						stateMap.erase(newIt);
					}
				}

				if (!Get().m_BufferBarriersCache.empty()) {
					return &Get().m_BufferBarriersCache;
				}

				return std::nullopt;
			}

			static bool OverrideBufferState(VulkanBuffer& buffer, const uint64_t offset, const uint64_t size, const ResourceState& desiredState) {
				CORI_PROFILE_FUNCTION();
				uint64_t handle = buffer.GetRawHandle();

				if (offset > buffer.m_Size || (offset + size > buffer.m_Size && size != VK_WHOLE_SIZE)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ResourceTracker }, "Invalid offset or size was provided when calling OverrideBufferState for buffer '{}', handle '{:#x}'. No barriers were generated.", buffer.m_Name, handle);
					return false;
				}

				auto it = Get().m_BufferStates.find(handle);
				if (it == Get().m_BufferStates.end()) {
					return false;
				}

				auto& stateMap = it->second;
				Get().m_BufferBarriersCache.clear();

				uint64_t newRangeStart = offset;
				uint64_t newRangeEnd = size == VK_WHOLE_SIZE ? buffer.m_Size : offset + size;

				auto currentIt = stateMap.upper_bound(newRangeStart);
				if (currentIt != stateMap.begin()) {
					--currentIt;
				}

				auto endIt = stateMap.lower_bound(newRangeEnd);
				if (endIt != stateMap.begin()) {
					auto prevIt = std::prev(endIt);

					bool sectorEnd = endIt == stateMap.end();

					uint64_t prevRangeEnd = sectorEnd ? buffer.m_Size : endIt->first;
					if (newRangeEnd < prevRangeEnd) {
						if (prevIt->second != desiredState) {
							stateMap[newRangeEnd] = prevIt->second;
						}
					}
				}

				stateMap.erase(stateMap.lower_bound(newRangeStart), stateMap.upper_bound(newRangeEnd));

				auto [newIt, _] = stateMap.emplace(newRangeStart, desiredState);

				auto nextIt = std::next(newIt);
				if (nextIt != stateMap.end() && nextIt->second == newIt->second) {
					stateMap.erase(nextIt);
				}

				if (newIt != stateMap.begin()) {
					auto prevIt = std::prev(newIt);
					if (prevIt->second == newIt->second) {
						stateMap.erase(newIt);
					}
				}

				return true;
			}

			static void RegisterImage(VulkanImage& image, vk::ImageLayout initialLayout) {
				CORI_PROFILE_FUNCTION();
				uint64_t handle = image.GetRawHandle();

				const ResourceState initialState = {
					.stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					.accessMask = vk::AccessFlagBits2::eNone,
					.imageLayout = initialLayout
				};

				auto& stateMap = Get().m_ImageStates[handle];
				stateMap.clear();

				if (vk::hasDepthComponent(image.m_Format) || vk::hasStencilComponent(image.m_Format)) {
					if (vk::hasDepthComponent(image.m_Format)) {
						for (uint32_t i = 0; i < image.m_ArrayLayers; ++i) {
							ImageSubresourceKey key = { i, vk::ImageAspectFlagBits::eDepth, 0 };
							stateMap[key] = initialState;
						}
					}

					if (vk::hasStencilComponent(image.m_Format)) {
						for (uint32_t i = 0; i < image.m_ArrayLayers; ++i) {
							ImageSubresourceKey key = { i, vk::ImageAspectFlagBits::eStencil, 0 };
							stateMap[key] = initialState;
						}
					}
				} else {
					for (uint32_t i = 0; i < image.m_ArrayLayers; ++i) {
						ImageSubresourceKey key = { i, vk::ImageAspectFlagBits::eColor, 0 };
						stateMap[key] = initialState;
					}
				}
			}

			static void RegisterBuffer(VulkanBuffer& buffer) {
				CORI_PROFILE_FUNCTION();
				uint64_t handle = buffer.GetRawHandle();

				constexpr ResourceState initialState = {
					.stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					.accessMask = vk::AccessFlagBits2::eNone,
					.imageLayout = vk::ImageLayout::eUndefined
				};

				auto& stateMap = Get().m_BufferStates[handle];
				stateMap.clear();

				stateMap[0] = initialState;
			}

			static void UnregisterImage(VulkanImage& image) {
				CORI_PROFILE_FUNCTION();
				uint64_t handle = image.GetRawHandle();
				Get().m_ImageStates.erase(handle);
			}

			static void UnregisterBuffer(VulkanBuffer& buffer) {
				CORI_PROFILE_FUNCTION();
				uint64_t handle = buffer.GetRawHandle();
				Get().m_BufferStates.erase(handle);
			}

		private:
			std::unordered_map<uint64_t, std::map<ImageSubresourceKey, ResourceState>> m_ImageStates;
			std::unordered_map<uint64_t, std::map<BufferSubresourceKey, ResourceState>> m_BufferStates;

			std::vector<vk::ImageMemoryBarrier2> m_ImageBarriersCache;
			std::vector<vk::BufferMemoryBarrier2> m_BufferBarriersCache;
			std::vector<vk::ImageAspectFlags> m_PendingImageAspectFlags;

			static constexpr vk::AccessFlags2 s_WriteFlagMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
													vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderStorageWrite |
													vk::AccessFlagBits2::eVideoDecodeWriteKHR | vk::AccessFlagBits2::eVideoEncodeWriteKHR | vk::AccessFlagBits2::eShaderTileAttachmentWriteQCOM |
													vk::AccessFlagBits2::eTransformFeedbackWriteEXT | vk::AccessFlagBits2::eTransformFeedbackWriteEXT | vk::AccessFlagBits2::eCommandPreprocessWriteEXT |
													vk::AccessFlagBits2::eCommandPreprocessWriteNV | vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureWriteNV | vk::AccessFlagBits2::eMicromapWriteEXT |
													vk::AccessFlagBits2::eOpticalFlowWriteNV | vk::AccessFlagBits2::eDataGraphWriteARM;
		};
	}
}

#endif