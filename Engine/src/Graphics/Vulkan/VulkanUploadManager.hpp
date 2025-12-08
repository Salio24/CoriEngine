#pragma once
#include "VulkanEngine.hpp"
#include "VulkanImage.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanEngine.hpp"
#include "Graphics/ResourceType.hpp"
#include "VulkanResourceTracker.hpp"
#include <sul/dynamic_bitset.hpp>

namespace Cori {
	namespace Graphics {

		#define HIGH_PRIORITY_STAGING_SIZE 96;
		#define LOW_PRIORITY_STAGING_SIZE 256;

		static uint64_t AlignUp(const uint64_t value, const uint64_t alignment) { //FIXME: move to helper
			return (value + alignment - 1) & ~(alignment - 1);
		}

		using AmazingBufferHandle = uint32_t;

		struct AmazingBuffer {
			struct CreateInfo {
				vk::BufferCreateFlags flags{};
				uint64_t size;
				bool createZeroed{ false };
				uint64_t updateCacheInitialSize{ 48 };
				vk::BufferUsageFlags usage;
				std::vector<uint32_t> queueFamilyIndices;
				const char* name{ "" };
			};

			struct UpdateData {
				uint64_t offset{ 0 };
				uint64_t alignment{ 4 };
				std::vector<Byte> data;
			};

			void SubmitUpdate(UpdateData&& update) {
				if (update.data.empty()) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::UploadManager }, "Submission was made for AmazingBuffer '{}', but the payload is empty, so no upload will be made.", m_Name);
				}

				update.offset = AlignUp(update.offset, update.alignment);

				CORI_CORE_ASSERT(update.data.size() + update.offset <= m_Size, "Amazing buffer master buffer update out of range.");

				m_PendingUpdates.emplace_back(std::move(update));
			}

			void Resize(const size_t newSize) {
				CORI_CORE_ASSERT(false, "not yet");

				m_MasterBufferResizeRequest.first = false;
				m_MasterBufferResizeRequest.second.newSize = newSize;
				m_MasterBufferResizeRequest.second.oldSize = m_Size;

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_FrameLocalBufferResizeRequests[i].first = false;
					m_FrameLocalBufferResizeRequests[i].second.newSize = newSize;
					m_FrameLocalBufferResizeRequests[i].second.oldSize = m_Size;
				}
			}

			VulkanBuffer& GetCurrentFrameLocalBuffer() {
				return m_FrameLocalBuffers[VulkanEngine::GetCurrentFrameInFlight()];
			}

		private:
			friend class VulkanUploadManager;

			struct ResizeRequest {
				uint32_t oldSize;
				uint32_t newSize;
			};

			void HostUpdate() {
				for (auto& update : m_PendingUpdates) {
					auto result = VulkanEngine::GetAllocator().copyMemoryToAllocation(update.data.data(), m_MasterBuffer.m_Allocation, update.offset, update.data.size());
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy update data to allocation of master buffer of amazing buffer '{}'. Error: {}", m_Name, vk::to_string(result));
				}

				m_PendingUpdates.clear();
			}

			bool ProcessSectors(std::vector<vk::BufferMemoryBarrier2>& barrierCache, const uint32_t frameIndex) {
				m_PendingBufferCopyRegions.clear();
				bool submitNeeded = false;

				for (auto& update : m_PendingUpdates) {
					uint32_t affectedSectorStart = floor(update.offset / m_SectorSize);
					uint32_t affectedSectorEnd = floor((update.offset + update.data.size() - 1) / m_SectorSize);

					for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
						m_SectorStates[i].set(affectedSectorStart, affectedSectorEnd - affectedSectorStart + 1, true);
					}
				}

				uint64_t currentRangeBeginning = m_SectorStates[frameIndex].find_first();
				while (currentRangeBeginning != sul::dynamic_bitset<>::npos) {
					submitNeeded = true;
					uint64_t currentRangeEnd = m_SectorStates[frameIndex].find_sequence_end(currentRangeBeginning);

					uint64_t offset = currentRangeBeginning * m_SectorSize;
					uint64_t size = (currentRangeEnd - currentRangeBeginning) * m_SectorSize;

					if (!m_EvenSectors && currentRangeEnd == m_SectorStates[frameIndex].size() - 1) {
						size += m_LastSectorSize;
					} else {
						size += m_SectorSize;
					}

					VulkanResourceTracker::OverrideBufferState(m_FrameLocalBuffers[frameIndex], offset, size, {vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone});
					auto barriers = VulkanResourceTracker::TransitionBuffer(m_FrameLocalBuffers[frameIndex], offset, size, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });

					if (barriers) {
						std::ranges::move(*barriers.value(), std::back_inserter(barrierCache));
					}

					m_PendingBufferCopyRegions.emplace_back(vk::BufferCopy{ offset, offset, size });

					currentRangeBeginning = m_SectorStates[frameIndex].find_next(currentRangeEnd);
				}

				m_SectorStates[frameIndex].reset();
				return submitNeeded;
			}

			void PerformCmbCopy(vk::CommandBuffer& cmb, const uint32_t frameIndex) {
				if (!m_PendingBufferCopyRegions.empty()) {
					cmb.copyBuffer(m_MasterBuffer.m_Buffer, m_FrameLocalBuffers[frameIndex].m_Buffer, m_PendingBufferCopyRegions.size(), m_PendingBufferCopyRegions.data());
				}
			}

			void Create(CreateInfo& createInfo) {
				bool named = false;

				#ifdef DEBUG_BUILD
					named = strcmp(createInfo.name, "") != 0;
				#endif

				if (named) {
					m_Name = createInfo.name;
				}

				CORI_CORE_ASSERT(createInfo.size != 0, "Trying to create AmazingBuffer '{}' with size of 0, this is illegal.", m_Name);

				//m_Size = AlignUp(createInfo.size, m_SectorSize);
				m_Size = createInfo.size;


				vk::BufferCreateInfo masterBufferCreateInfo {
					.flags = createInfo.flags,
					.size = m_Size,
					.usage = createInfo.usage | vk::BufferUsageFlagBits::eTransferSrc,
					.sharingMode = vk::SharingMode::eExclusive,
				};

				vma::AllocationCreateInfo masterAllocationCreateInfo {
					.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead | vma::AllocationCreateFlagBits::eMapped,
					.usage = vma::MemoryUsage::eAuto,
					.requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible
				};


				vk::BufferCreateInfo frameLocalBufferCreateInfo {
					.flags = createInfo.flags,
					.size = m_Size,
					.usage = createInfo.usage | vk::BufferUsageFlagBits::eTransferDst
				};

				uint32_t transferQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex();

				bool transferQueueInVector = false;
				for (auto familyIndex : createInfo.queueFamilyIndices) {
					if (familyIndex == transferQueueFamilyIndex) {
						transferQueueInVector = true;
					}
				}

				if (transferQueueInVector && createInfo.queueFamilyIndices.size() == 1) {
					frameLocalBufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
				} else if (transferQueueInVector && createInfo.queueFamilyIndices.size() != 1) {
					frameLocalBufferCreateInfo.sharingMode = vk::SharingMode::eConcurrent;
					frameLocalBufferCreateInfo.queueFamilyIndexCount = createInfo.queueFamilyIndices.size();
					frameLocalBufferCreateInfo.pQueueFamilyIndices = createInfo.queueFamilyIndices.data();
					m_QueueFamilyIndices = createInfo.queueFamilyIndices;
				} else {
					createInfo.queueFamilyIndices.emplace_back(transferQueueFamilyIndex);
					frameLocalBufferCreateInfo.sharingMode = vk::SharingMode::eConcurrent;
					frameLocalBufferCreateInfo.queueFamilyIndexCount = createInfo.queueFamilyIndices.size();
					frameLocalBufferCreateInfo.pQueueFamilyIndices = createInfo.queueFamilyIndices.data();
					m_QueueFamilyIndices = createInfo.queueFamilyIndices;
				}

				vma::AllocationCreateInfo frameLocalAllocationCreateInfo {
					.flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
					.usage = vma::MemoryUsage::eAuto
				};

				std::string masterName = named ? std::format("Master buffer of amazing buffer: {}", createInfo.name) : "";

				VulkanBuffer::CreateInfo masterInfo {
					.bufferCreateInfo = &masterBufferCreateInfo,
					.allocationCreateInfo = &masterAllocationCreateInfo,
				};

				if (named) {
					masterInfo.name = masterName.c_str();
				}

				m_MasterBuffer = VulkanBuffer::Create(masterInfo);

				vk::MemoryPropertyFlags masterBufferFlags = VulkanEngine::GetAllocator().getAllocationMemoryProperties(m_MasterBuffer.m_Allocation);

				CORI_CORE_ASSERT(masterBufferFlags & vk::MemoryPropertyFlagBits::eHostVisible, "Master buffer of amazing buffer ended up in not host visible memory.");

				if (!(masterBufferFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::UploadManager }, "Master buffer of amazing buffer '{}' ended up in non DEVICE_LOCAL memory (failed to allocate in BAR), and in system RAM instead, performance will likely be degraded.", m_Name);
				}

				if (createInfo.createZeroed) {
					auto [result, ptr] = VulkanEngine::GetAllocator().mapMemory(m_MasterBuffer.m_Allocation);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to map memory of the allocated master buffer when creating amazing buffer '{}'. Error: {}", createInfo.name, vk::to_string(result));

					memset(ptr, 0, m_Size);

					VulkanEngine::GetAllocator().unmapMemory(m_MasterBuffer.m_Allocation);
					result = VulkanEngine::GetAllocator().flushAllocation(m_MasterBuffer.m_Allocation, 0, m_Size);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to flush allocation of master buffer of amazing buffer '{}' after zeroing out memory. Error: {}", createInfo.name, vk::to_string(result));
				}

				uint32_t sectorCount = ceil(static_cast<float>(m_Size) / static_cast<float>(m_SectorSize));
				if (m_Size % m_SectorSize != 0) {
					m_LastSectorSize = m_Size % m_SectorSize;
				} else {
					m_EvenSectors = true;
				}

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					std::string localName = named ? std::format("Local frame '{}' buffer of amazing buffer: {}", i, createInfo.name) : "";

					VulkanBuffer::CreateInfo frameLocalInfo {
						.bufferCreateInfo = &frameLocalBufferCreateInfo,
						.allocationCreateInfo = &frameLocalAllocationCreateInfo,
					};

					if (named) {
						frameLocalInfo.name = localName.c_str();
					}

					m_FrameLocalBuffers[i] = VulkanBuffer::Create(frameLocalInfo);
					m_SectorStates[i].resize(sectorCount);
				}

				m_CreateFlags = createInfo.flags;
				m_UsageFlags = createInfo.usage;

				m_PendingUpdates.reserve(createInfo.updateCacheInitialSize);
				m_PendingBufferCopyRegions.reserve(createInfo.updateCacheInitialSize);
				m_QueueFamilyIndices.reserve(4);

				m_Valid = true;
			}

			void Destroy() {
				m_PendingUpdates.clear();
				m_PendingBufferCopyRegions.clear();
				m_QueueFamilyIndices.clear();
				m_PendingUpdates.shrink_to_fit();
				m_PendingBufferCopyRegions.shrink_to_fit();
				m_QueueFamilyIndices.shrink_to_fit();

				m_UsageFlags = vk::BufferUsageFlags{};
				m_CreateFlags = vk::BufferCreateFlags{};
				m_Name = "Unnamed Amazing Buffer";

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_SectorStates[i].clear();
					m_SectorStates[i].shrink_to_fit();
					m_FrameLocalBuffers[i].Destroy();
				}

				m_MasterBuffer.Destroy();

				m_Size = 0;
				m_EvenSectors = false;
				m_Valid = false;
				m_LastSectorSize = 0;
			}

			bool IsValid() {
				return m_Valid;
			}

			std::vector<UpdateData> m_PendingUpdates;
			std::vector<vk::BufferCopy> m_PendingBufferCopyRegions;
			VulkanBuffer m_MasterBuffer;
			std::pair<bool, ResizeRequest> m_MasterBufferResizeRequest;

			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_FrameLocalBuffers;
			std::array<sul::dynamic_bitset<>, FRAMES_IN_FLIGHT> m_SectorStates;

			std::array<std::pair<bool, ResizeRequest>, FRAMES_IN_FLIGHT> m_FrameLocalBufferResizeRequests;

			static constexpr uint32_t m_SectorSize{ 1024 * 16 };
			bool m_EvenSectors{ false };
			bool m_Valid{ false };
			uint32_t m_LastSectorSize{ 0 };

			uint64_t m_Size{ 0 };
			const char* m_Name{ "Unnamed Amazing Buffer" };
			vk::BufferCreateFlags m_CreateFlags;
			vk::BufferUsageFlags m_UsageFlags;
			std::vector<uint32_t> m_QueueFamilyIndices;
		};

		class VulkanUploadManager {
		public:
			struct ImageUploadRange {
				vk::Offset3D offset;
				vk::Extent3D extent;
				vk::ImageSubresourceLayers subresourceLayers;
			};

			struct BufferUploadRange {
				uint64_t offset{ 0 };
				uint64_t alignment{ 4 };
			};

			enum class UploadType {
				Undefined,
				FrameCritical,
				Streaming
			};

			struct UploadPart {
				std::variant<VulkanImage, VulkanBuffer> resource;
				std::variant<ImageUploadRange, BufferUploadRange> range;
				std::vector<Byte> data;
			};

			struct UploadRequest {
				std::variant<std::vector<UploadPart>, UploadPart> uploadParts;
				std::function<void(void*)> callback{};
				UploadType uploadType{};
				void* userData{};
			};

			static void Init();

			static void Shutdown();

			static VulkanUploadManager& Get();

			static AmazingBufferHandle CreateAmazingBuffer(AmazingBuffer::CreateInfo& createInfo) {
				if (!Get().m_Holes.empty()) {
					AmazingBufferHandle freeHandle = Get().m_Holes.back();
					Get().m_Holes.pop_back();

					Get().m_AmazingBuffers[freeHandle].Create(createInfo);
					return freeHandle;
				}

				AmazingBufferHandle newHandle = Get().m_AmazingBuffers.size();
				auto& buffer = Get().m_AmazingBuffers.emplace_back();
				buffer.Create(createInfo);
				return newHandle;
			}

			static AmazingBuffer& GetAmazingBuffer(const AmazingBufferHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_AmazingBuffers.size(), "Invalid AmazingBufferHandle was passed to VulkanUploadManager::GetAmazingBuffer.");
				return Get().m_AmazingBuffers[handle];
			}

			static void DestroyAmazingBuffer(AmazingBufferHandle& handle) {
				CORI_CORE_ASSERT(handle < Get().m_AmazingBuffers.size(), "Invalid AmazingBufferHandle was passed to VulkanUploadManager::GetAmazingBuffer.");
				Get().m_AmazingBuffers[handle].Destroy();
				Get().m_Holes.emplace_back(handle);
				handle = UINT32_MAX;
			}

			static void SubmitUploadRequest(UploadRequest&& request) {
				static auto CheckEmpty = [](UploadPart& part) -> bool {
					if (part.data.empty()) {
						if (std::holds_alternative<VulkanBuffer>(part.resource)) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::UploadManager }, "Submission was made for VulkanBuffer '{}', but the payload is empty, so no upload will be made.", std::get<VulkanBuffer>(part.resource).m_Name);
							return true;
						}

						CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::UploadManager }, "Submission was made for VulkanImage '{}', but the payload is empty, so no upload will be made.", std::get<VulkanImage>(part.resource).m_Name);
						return true;
					}

					return false;
				};

				if (std::holds_alternative<UploadPart>(request.uploadParts)) {
					if (CheckEmpty(std::get<UploadPart>(request.uploadParts))) {
						return;
					}
				} else {
					for (auto& part : std::get<std::vector<UploadPart>>(request.uploadParts)) {
						if (CheckEmpty(part)) {
							return;
						}
					}
				}

				switch (request.uploadType) {
				case UploadType::FrameCritical:
					{
						Get().m_HighPriorityUploads.emplace_back(std::move(request));
						break;
					}
				case UploadType::Streaming:
					{
						Get().m_LowPriorityUploads.emplace(std::move(request));
						break;
					}
				default:
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::UploadManager }, "UploadType was not specified in UploadRequest when calling SubmitRequest, no upload will be made.");
				}
			}

			~VulkanUploadManager() {
				auto device = VulkanEngine::GetLogicalDevice();
				auto result = device.waitIdle();
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Calling wait idle on device has failed. Error: {}", vk::to_string(result));

				for (auto& amazingBuffer : m_AmazingBuffers) {
					if (amazingBuffer.IsValid()) {
						amazingBuffer.Destroy();
					}
				}

				m_HighPriorityBlock.clearVirtualBlock();
				m_LowPriorityBlock.clearVirtualBlock();

				m_HighPriorityBlock.destroy();
				m_LowPriorityBlock.destroy();

				m_HighPriorityRingStagingBuffer.Destroy();
				m_LowPriorityRingStagingBuffer.Destroy();

				device.destroyFence(m_AmazingBuffersFence);
				device.destroySemaphore(m_AmazingSemaphore);

				auto& transferCmp = VulkanEngine::GetTransferCmp();

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
					device.freeCommandBuffers(transferCmp, m_HighPriorityPrimaryCommandBuffers[i]);
					device.freeCommandBuffers(transferCmp, m_LowPriorityPrimaryCommandBuffers[i]);
					device.freeCommandBuffers(transferCmp, m_AmazingBuffersPrimaryCommandBuffers[i]);
					device.freeCommandBuffers(transferCmp, m_HighPrioritySecondaryCommandBuffers[i]);
					device.freeCommandBuffers(transferCmp, m_LowPrioritySecondaryCommandBuffers[i]);

					device.destroyFence(m_HighPriorityFences[i]);
					device.destroyFence(m_LowPriorityFences[i]);

					device.destroySemaphore(m_HighPrioritySubmitSemaphores[i]);
					device.destroySemaphore(m_LowPrioritySubmitSemaphores[i]);
				}
			}

		protected:
			friend VulkanEngine;

			void SubmitStaging() {
				CORI_PROFILE_FUNCTION();
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();

				uint32_t transferQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex();

				Reclaim();

				vk::CommandBufferBeginInfo primaryBeginInfo {
					.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
				};

				vk::CommandBufferInheritanceInfo inherit{};
				inherit.pNext = nullptr;
				inherit.occlusionQueryEnable = vk::False;


				vk::CommandBufferBeginInfo secondaryBeginInfo {
					.pInheritanceInfo = &inherit,
				};


				if (!m_HighPriorityUploads.empty()) {
					auto result = m_HighPrioritySecondaryCommandBuffers[frameIndex].begin(secondaryBeginInfo);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin high priority secondary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));
					m_BufferBarriersCache.clear();
					m_ImageBarriersCache.clear();

					auto ProcessPartHigh = [&](UploadPart& part) {
						if (std::holds_alternative<VulkanImage>(part.resource)) {
							CORI_CORE_ASSERT(std::holds_alternative<ImageUploadRange>(part.range), "UploadPart was passed to VulkanUploadManager with VulkanImage as a resource and BufferUploadRange as a range.");
							auto& range = std::get<ImageUploadRange>(part.range);
							auto& image = std::get<VulkanImage>(part.resource);

							vk::BufferImageCopy region{
								.bufferRowLength = 0,
								.bufferImageHeight = 0,
								.imageSubresource = range.subresourceLayers,
							};

							std::array<uint8_t, 3> blockExtent = vk::blockExtent(image.m_Format);

							region.imageOffset = vk::Offset3D{ static_cast<int32_t>(std::clamp(static_cast<uint32_t>(AlignUp(range.offset.x, blockExtent[0])), 0u, image.m_Extent3D.width)),
									static_cast<int32_t>(std::clamp(static_cast<uint32_t>(AlignUp(range.offset.y, blockExtent[1])), 0u, image.m_Extent3D.height)),
									static_cast<int32_t>(std::clamp(static_cast<uint32_t>(AlignUp(range.offset.z, blockExtent[2])), 0u, image.m_Extent3D.depth)) };

							CORI_CORE_ASSERT(!(region.imageOffset.x == static_cast<int32_t>(image.m_Extent3D.width) || region.imageOffset.y == static_cast<int32_t>(image.m_Extent3D.height) || region.imageOffset.z == static_cast<int32_t>(image.m_Extent3D.depth)), "Invalid ImageUploadRange no upload will be made, frame critical queue -> aborting. Image offset ended up at the image edge after block extent alignment. Requested offset: '{} {} {}', aligned offset '{} {} {}', Image '{}'", range.offset.x, range.offset.y, range.offset.z, region.imageOffset.x, region.imageOffset.y, region.imageOffset.z, image.m_Name);

							region.imageExtent = vk::Extent3D{ std::clamp(static_cast<uint32_t>(AlignUp(range.extent.width, blockExtent[0])), 0u, image.m_Extent3D.width - region.imageOffset.x),
																std::clamp(static_cast<uint32_t>(AlignUp(range.extent.height, blockExtent[1])), 0u, image.m_Extent3D.height - region.imageOffset.y),
																std::clamp(static_cast<uint32_t>(AlignUp(range.extent.depth, blockExtent[2])), 0u, image.m_Extent3D.depth - region.imageOffset.z) };

							std::optional<Allocation> alloc = AllocateHighPriority(part.data.size(), std::max<uint64_t>(4, vk::blockSize(image.m_Format)));
							CORI_CORE_ASSERT(alloc, "High priority ring staging buffer out of space, failed to allocate memory for uploading frame critical data.");
							region.bufferOffset = alloc->offset;

							m_PendingHighPriorityUploads.emplace(alloc.value().virtualAllocation, frameIndex);

							VulkanResourceTracker::OverrideBufferState(m_HighPriorityRingStagingBuffer, alloc->offset, part.data.size(), {vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostWrite});
							auto result_ = VulkanEngine::GetAllocator().copyMemoryToAllocation(part.data.data(), m_HighPriorityRingStagingBuffer.m_Allocation, alloc->offset, part.data.size());
							CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to copy frame critical data to the staging buffer. Error: {}", vk::to_string(result_));

							vk::ImageSubresourceRange vkRange{
								.aspectMask = range.subresourceLayers.aspectMask,
								.baseMipLevel = range.subresourceLayers.mipLevel,
								.levelCount = 1,
								.baseArrayLayer = range.subresourceLayers.baseArrayLayer,
								.layerCount = range.subresourceLayers.layerCount
							};

							//TODO: override image state

							auto dstBarrier = VulkanResourceTracker::TransitionImage(image, vkRange, {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal });

							if (dstBarrier) {
								std::ranges::move(*dstBarrier.value(), std::back_inserter(m_ImageBarriersCache));
							}

							auto stageBarrier = VulkanResourceTracker::TransitionBuffer(m_HighPriorityRingStagingBuffer, alloc->offset, part.data.size(), {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead});

							if (stageBarrier) {
								std::ranges::move(*stageBarrier.value(), std::back_inserter(m_BufferBarriersCache));
							}

							m_HighPrioritySecondaryCommandBuffers[frameIndex].copyBufferToImage(m_HighPriorityRingStagingBuffer.m_Buffer, image.m_Image, vk::ImageLayout::eTransferDstOptimal, region);
						}
						else if (std::holds_alternative<VulkanBuffer>(part.resource)) {
							CORI_CORE_ASSERT(std::holds_alternative<BufferUploadRange>(part.range), "UploadPart was passed to VulkanUploadManager with VulkanBuffer as a resource and ImageUploadRange as a range.");
							auto& range = std::get<BufferUploadRange>(part.range);
							auto& buffer = std::get<VulkanBuffer>(part.resource);

							std::optional<Allocation> alloc = AllocateHighPriority(part.data.size(), std::max<uint64_t>(4, range.alignment));
							CORI_CORE_ASSERT(alloc, "High priority ring staging buffer out of space, failed to allocate memory for uploading frame critical data.");
							m_PendingHighPriorityUploads.emplace(alloc.value().virtualAllocation, frameIndex);

							VulkanResourceTracker::OverrideBufferState(m_HighPriorityRingStagingBuffer, alloc->offset, part.data.size(), {vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostWrite});
							result = VulkanEngine::GetAllocator().copyMemoryToAllocation(part.data.data(), m_HighPriorityRingStagingBuffer.m_Allocation, alloc->offset, part.data.size());
							CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy frame critical data to the staging buffer. Error: {}", vk::to_string(result));

							ResourceState dstState{
								.stageMask = vk::PipelineStageFlagBits2::eTransfer,
								.accessMask = vk::AccessFlagBits2::eTransferWrite
							};

							VulkanResourceTracker::OverrideBufferState(buffer, range.offset, part.data.size(), {vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone});
							auto dstBarrier = VulkanResourceTracker::TransitionBuffer(buffer, range.offset, part.data.size(), dstState);

							if (dstBarrier) {
								std::ranges::move(*dstBarrier.value(), std::back_inserter(m_BufferBarriersCache));
							}

							auto stageBarrier = VulkanResourceTracker::TransitionBuffer(m_HighPriorityRingStagingBuffer, alloc->offset, part.data.size(), {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead});

							if (stageBarrier) {
								std::ranges::move(*stageBarrier.value(), std::back_inserter(m_BufferBarriersCache));
							}

							vk::BufferCopy region{
								.srcOffset = alloc->offset,
								.dstOffset = range.offset,
								.size = part.data.size(),
							};

							m_HighPrioritySecondaryCommandBuffers[frameIndex].copyBuffer(m_HighPriorityRingStagingBuffer.m_Buffer, buffer.m_Buffer, region);
						}
					};

					for (auto& upload : m_HighPriorityUploads) {
						if (std::holds_alternative<std::vector<UploadPart>>(upload.uploadParts)) {
							auto& list = std::get<std::vector<UploadPart>>(upload.uploadParts);
							for (auto& part : list) {
								ProcessPartHigh(part);
							}
						} else if (std::holds_alternative<UploadPart>(upload.uploadParts)) {
							ProcessPartHigh(std::get<UploadPart>(upload.uploadParts));
						}
					}


					result = m_HighPrioritySecondaryCommandBuffers[frameIndex].end();
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end high priority secondary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));


					result = m_HighPriorityPrimaryCommandBuffers[frameIndex].begin(primaryBeginInfo);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin high priority primary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));

					vk::DependencyInfo depInfo {
						.bufferMemoryBarrierCount = static_cast<uint32_t>(m_BufferBarriersCache.size()),
						.pBufferMemoryBarriers = m_BufferBarriersCache.data(),
						.imageMemoryBarrierCount = static_cast<uint32_t>(m_ImageBarriersCache.size()),
						.pImageMemoryBarriers = m_ImageBarriersCache.data(),
					};

					m_HighPriorityPrimaryCommandBuffers[frameIndex].pipelineBarrier2(depInfo);

					m_HighPriorityPrimaryCommandBuffers[frameIndex].executeCommands(m_HighPrioritySecondaryCommandBuffers[frameIndex]);


					result = m_HighPriorityPrimaryCommandBuffers[frameIndex].end();
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end high priority primary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));

					result = VulkanEngine::GetLogicalDevice().resetFences(m_HighPriorityFences[frameIndex]);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to reset fence. Error: {}", vk::to_string(result));

					vk::SubmitInfo submitInfo{
						.commandBufferCount = 1,
						.pCommandBuffers = &m_HighPriorityPrimaryCommandBuffers[frameIndex],
						.signalSemaphoreCount = 1,
						.pSignalSemaphores = &m_HighPrioritySubmitSemaphores[frameIndex]
					};

					result = VulkanEngine::GetTransferQueue().submit(submitInfo, m_HighPriorityFences[frameIndex]);
					VulkanEngine::AddWaitSemaphore(m_HighPrioritySubmitSemaphores[frameIndex], vk::PipelineStageFlagBits::eAllCommands);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Transfer queue submission failed. Error: {}", vk::to_string(result));
					m_HighPriorityUploads.clear();
				}

				if (!m_LowPriorityUploads.empty()) {
					auto result = m_LowPrioritySecondaryCommandBuffers[frameIndex].begin(secondaryBeginInfo);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin low priority secondary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));
					m_BufferBarriersCache.clear();
					m_ImageBarriersCache.clear();

					auto ProcessPartLow = [&](UploadPart& part, Allocation& alloc) {
						if (std::holds_alternative<VulkanImage>(part.resource)) {
							CORI_CORE_ASSERT(std::holds_alternative<ImageUploadRange>(part.range), "UploadPart was passed to VulkanUploadManager with VulkanImage as a resource and BufferUploadRange as a range.");
							auto& range = std::get<ImageUploadRange>(part.range);
							auto& image = std::get<VulkanImage>(part.resource);

							vk::BufferImageCopy region{
								.bufferRowLength = 0,
								.bufferImageHeight = 0,
								.imageSubresource = range.subresourceLayers,
							};

							std::array<uint8_t, 3> blockExtent = vk::blockExtent(image.m_Format);

							region.imageOffset = vk::Offset3D{ static_cast<int32_t>(std::clamp(static_cast<uint32_t>(AlignUp(range.offset.x, blockExtent[0])), 0u, image.m_Extent3D.width)),
									static_cast<int32_t>(std::clamp(static_cast<uint32_t>(AlignUp(range.offset.y, blockExtent[1])), 0u, image.m_Extent3D.height)),
									static_cast<int32_t>(std::clamp(static_cast<uint32_t>(AlignUp(range.offset.z, blockExtent[2])), 0u, image.m_Extent3D.depth)) };

							if (region.imageOffset.x == static_cast<int32_t>(image.m_Extent3D.width) || region.imageOffset.y == static_cast<int32_t>(image.m_Extent3D.height) || region.imageOffset.z == static_cast<int32_t>(image.m_Extent3D.depth)) {
								CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::UploadManager }, "Invalid ImageUploadRange no upload will be made. Image offset ended up at the image edge after block extent alignment. Requested offset: '{} {} {}', aligned offset '{} {} {}', Image '{}'", range.offset.x, range.offset.y, range.offset.z, region.imageOffset.x, region.imageOffset.y, region.imageOffset.z, image.m_Name);
								return;
							}

							region.imageExtent = vk::Extent3D{ std::clamp(static_cast<uint32_t>(AlignUp(range.extent.width, blockExtent[0])), 0u, image.m_Extent3D.width - region.imageOffset.x),
																std::clamp(static_cast<uint32_t>(AlignUp(range.extent.height, blockExtent[1])), 0u, image.m_Extent3D.height - region.imageOffset.y),
																std::clamp(static_cast<uint32_t>(AlignUp(range.extent.depth, blockExtent[2])), 0u, image.m_Extent3D.depth - region.imageOffset.z) };

							region.bufferOffset = alloc.offset;

							m_PendingLowPriorityUploads.emplace(alloc.virtualAllocation, frameIndex);
							VulkanResourceTracker::OverrideBufferState(m_LowPriorityRingStagingBuffer, alloc.offset, part.data.size(), {vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostWrite});
							auto result_ = VulkanEngine::GetAllocator().copyMemoryToAllocation(part.data.data(), m_LowPriorityRingStagingBuffer.m_Allocation, alloc.offset, part.data.size());
							CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to copy streaming data to the staging buffer. Error: {}", vk::to_string(result_));

							vk::ImageSubresourceRange vkRange{
								.aspectMask = range.subresourceLayers.aspectMask,
								.baseMipLevel = range.subresourceLayers.mipLevel,
								.levelCount = 1,
								.baseArrayLayer = range.subresourceLayers.baseArrayLayer,
								.layerCount = range.subresourceLayers.layerCount
							};

							auto dstBarrier = VulkanResourceTracker::TransitionImage(image, vkRange, {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal });

							if (dstBarrier) {
								std::ranges::move(*dstBarrier.value(), std::back_inserter(m_ImageBarriersCache));
							}

							auto stageBarrier = VulkanResourceTracker::TransitionBuffer(m_LowPriorityRingStagingBuffer, alloc.offset, part.data.size(), {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead});

							if (stageBarrier) {
								std::ranges::move(*stageBarrier.value(), std::back_inserter(m_BufferBarriersCache));
							}

							m_LowPrioritySecondaryCommandBuffers[frameIndex].copyBufferToImage(m_LowPriorityRingStagingBuffer.m_Buffer, image.m_Image, vk::ImageLayout::eTransferDstOptimal, region);
						}
						else if (std::holds_alternative<VulkanBuffer>(part.resource)) {
							CORI_CORE_ASSERT(std::holds_alternative<BufferUploadRange>(part.range), "UploadPart was passed to VulkanUploadManager with VulkanBuffer as a resource and ImageUploadRange as a range.");
							auto& range = std::get<BufferUploadRange>(part.range);
							auto& buffer = std::get<VulkanBuffer>(part.resource);

							m_PendingLowPriorityUploads.emplace(alloc.virtualAllocation, frameIndex);

							VulkanResourceTracker::OverrideBufferState(m_LowPriorityRingStagingBuffer, alloc.offset, part.data.size(), {vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostWrite});
							auto result_ = VulkanEngine::GetAllocator().copyMemoryToAllocation(part.data.data(), m_LowPriorityRingStagingBuffer.m_Allocation, alloc.offset, part.data.size());
							CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to copy streaming data to the staging buffer. Error: {}", vk::to_string(result_));

							ResourceState dstState{
								.stageMask = vk::PipelineStageFlagBits2::eTransfer,
								.accessMask = vk::AccessFlagBits2::eTransferWrite
							};

							VulkanResourceTracker::OverrideBufferState(buffer, range.offset, part.data.size(), {vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone});
							auto dstBarrier = VulkanResourceTracker::TransitionBuffer(buffer, range.offset, part.data.size(), dstState);

							if (dstBarrier) {
								std::ranges::move(*dstBarrier.value(), std::back_inserter(m_BufferBarriersCache));
							}

							auto stageBarrier = VulkanResourceTracker::TransitionBuffer(m_LowPriorityRingStagingBuffer, alloc.offset, part.data.size(), {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead});

							if (stageBarrier) {
								std::ranges::move(*stageBarrier.value(), std::back_inserter(m_BufferBarriersCache));
							}

							vk::BufferCopy region{
								.srcOffset = alloc.offset,
								.dstOffset = range.offset,
								.size = part.data.size(),
							};

							m_LowPrioritySecondaryCommandBuffers[frameIndex].copyBuffer(m_LowPriorityRingStagingBuffer.m_Buffer, buffer.m_Buffer, region);
						}
					};

					while (!m_LowPriorityUploads.empty()) {
						auto& upload = m_LowPriorityUploads.front();

						static auto Allocate = [&](UploadPart& part) -> std::optional<Allocation> {
							if (std::holds_alternative<VulkanImage>(part.resource)) {
								return AllocateLowPriority(part.data.size(), std::max<uint64_t>(4, vk::blockSize(std::get<VulkanImage>(part.resource).m_Format)));
							}

							if (std::holds_alternative<VulkanBuffer>(part.resource)) {
								return AllocateLowPriority(part.data.size(), std::max<uint64_t>(4, std::get<BufferUploadRange>(part.range).alignment));
							}

							return std::nullopt;
						};



						if (std::holds_alternative<std::vector<UploadPart>>(upload.uploadParts)) {
							auto& list = std::get<std::vector<UploadPart>>(upload.uploadParts);

							bool allocFailed = false;
							m_TempAllocs.clear();

							for (auto& part : list) {
								auto alloc = Allocate(part);
								if (alloc) {
									m_TempAllocs.emplace_back(*alloc);
								} else {
									allocFailed = true;
									break;
								}
							}

							if (allocFailed) {
								for (auto& alloc : m_TempAllocs) {
									m_LowPriorityBlock.virtualFree(alloc.virtualAllocation);
								}

								break;
							}

							for (auto [part , alloc] : std::ranges::zip_view(list, m_TempAllocs)) {
								ProcessPartLow(part, alloc);
							}
						}
						else if (std::holds_alternative<UploadPart>(upload.uploadParts)) {
							auto alloc = Allocate(std::get<UploadPart>(upload.uploadParts));
							if (alloc) {
								ProcessPartLow(std::get<UploadPart>(upload.uploadParts), alloc.value());
							} else {
								break;
							}
						}

						if (upload.callback) {
							upload.callback(upload.userData);
						}

						m_LowPriorityUploads.pop();
					}

					result = m_LowPrioritySecondaryCommandBuffers[frameIndex].end();
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end low priority secondary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));
					result = m_LowPriorityPrimaryCommandBuffers[frameIndex].begin(primaryBeginInfo);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin low priority primary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));

					vk::DependencyInfo depInfo_ {
						.bufferMemoryBarrierCount = static_cast<uint32_t>(m_BufferBarriersCache.size()),
						.pBufferMemoryBarriers = m_BufferBarriersCache.data(),
						.imageMemoryBarrierCount = static_cast<uint32_t>(m_ImageBarriersCache.size()),
						.pImageMemoryBarriers = m_ImageBarriersCache.data(),
					};

					m_LowPriorityPrimaryCommandBuffers[frameIndex].pipelineBarrier2(depInfo_);

					m_LowPriorityPrimaryCommandBuffers[frameIndex].executeCommands(m_LowPrioritySecondaryCommandBuffers[frameIndex]);

					result = m_LowPriorityPrimaryCommandBuffers[frameIndex].end();
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end low priority primary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));

					result = VulkanEngine::GetLogicalDevice().resetFences(m_LowPriorityFences[frameIndex]);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to reset fence. Error: {}", vk::to_string(result));

					vk::SubmitInfo submitInfo_{
							.commandBufferCount = 1,
							.pCommandBuffers = &m_LowPriorityPrimaryCommandBuffers[frameIndex],
							.signalSemaphoreCount = 1,
							.pSignalSemaphores = &m_LowPrioritySubmitSemaphores[frameIndex]
					};

					result = VulkanEngine::GetTransferQueue().submit(submitInfo_, m_LowPriorityFences[frameIndex]);
					VulkanEngine::AddWaitSemaphore(m_LowPrioritySubmitSemaphores[frameIndex], vk::PipelineStageFlagBits::eAllCommands);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Transfer queue submission failed. Error: {}", vk::to_string(result));
					//m_NextTimelineValue++;
				}
			}

			void SubmitAmazing() {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				bool submitNeeded = false;
				m_BufferBarriersCache.clear();

				for (auto& amazingBuffer : m_AmazingBuffers) {
					if (amazingBuffer.IsValid()) {
						bool result = amazingBuffer.ProcessSectors(m_BufferBarriersCache, frameIndex);
						if (!submitNeeded) {
							submitNeeded = result;
						}
					}
				}

				if (submitNeeded) {
					vk::CommandBufferBeginInfo beginInfo {
						.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
					};

					auto result = m_AmazingBuffersPrimaryCommandBuffers[frameIndex].begin(beginInfo);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin amazing buffer primary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));

					if (!m_BufferBarriersCache.empty()) {
						vk::DependencyInfo depInfo {
							.bufferMemoryBarrierCount = static_cast<uint32_t>(m_BufferBarriersCache.size()),
							.pBufferMemoryBarriers = m_BufferBarriersCache.data()
						};

						m_AmazingBuffersPrimaryCommandBuffers[frameIndex].pipelineBarrier2(depInfo);
					}

					for (auto& amazingBuffer : m_AmazingBuffers) {
						if (amazingBuffer.IsValid()) {
							amazingBuffer.PerformCmbCopy(m_AmazingBuffersPrimaryCommandBuffers[frameIndex], frameIndex);
						}
					}

					result = m_AmazingBuffersPrimaryCommandBuffers[frameIndex].end();
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end amazing buffer primary command buffer '{}' recording in UploadManager. Error: {}", frameIndex, vk::to_string(result));

					while (vk::Result::eTimeout == VulkanEngine::GetLogicalDevice().waitForFences(m_AmazingBuffersFence, vk::True, UINT64_MAX)) {}
					result = VulkanEngine::GetLogicalDevice().resetFences(m_AmazingBuffersFence);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to reset fence. Error: {}", vk::to_string(result));

					for (auto& amazingBuffer : m_AmazingBuffers) {
						if (amazingBuffer.IsValid()) {
							amazingBuffer.HostUpdate();
						}
					}

					vk::SubmitInfo submitInfo{
						.commandBufferCount = 1,
						.pCommandBuffers = &m_AmazingBuffersPrimaryCommandBuffers[frameIndex],
						.signalSemaphoreCount = 1,
						.pSignalSemaphores = &m_AmazingSemaphore
					};

					result = VulkanEngine::GetTransferQueue().submit(submitInfo, m_AmazingBuffersFence);
					VulkanEngine::AddWaitSemaphore(m_AmazingSemaphore, vk::PipelineStageFlagBits::eAllCommands);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Transfer queue submission failed. Error: {}", vk::to_string(result));
				}

			}

		private:
			VulkanUploadManager() {
				vk::CommandBufferAllocateInfo pCmbCreateInfo {
					.commandPool = VulkanEngine::GetTransferCmp(),
					.level = vk::CommandBufferLevel::ePrimary,
					.commandBufferCount = FRAMES_IN_FLIGHT * 3
				};

				vk::CommandBufferAllocateInfo sCmbCreateInfo {
					.commandPool = VulkanEngine::GetTransferCmp(),
					.level = vk::CommandBufferLevel::eSecondary,
					.commandBufferCount = FRAMES_IN_FLIGHT * 2
				};

				auto [result, pCmb] = VulkanEngine::GetLogicalDevice().allocateCommandBuffers(pCmbCreateInfo);
				auto [result1, sCmb] = VulkanEngine::GetLogicalDevice().allocateCommandBuffers(sCmbCreateInfo);

				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create command buffers. Error: {}", vk::to_string(result));
				CORI_CORE_ASSERT(result1 == vk::Result::eSuccess, "Failed to create command buffers. Error: {}", vk::to_string(result1));

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					auto [result2, semaphore] = VulkanEngine::GetLogicalDevice().createSemaphore(vk::SemaphoreCreateInfo());
					auto [result3, semaphore_] = VulkanEngine::GetLogicalDevice().createSemaphore(vk::SemaphoreCreateInfo());

					CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "Failed to create semaphore. Error: {}", vk::to_string(result2));
					CORI_CORE_ASSERT(result3 == vk::Result::eSuccess, "Failed to create semaphore. Error: {}", vk::to_string(result3));

					VulkanEngine::SetDebugName(semaphore, std::format("VulkanUploadManager HighPriority Semaphore {}", i));
					VulkanEngine::SetDebugName(semaphore_, std::format("VulkanUploadManager LowPriority Semaphore {}", i));

					m_HighPrioritySubmitSemaphores[i] = semaphore;
					m_LowPrioritySubmitSemaphores[i] = semaphore_;

					auto [result4, fence] = VulkanEngine::GetLogicalDevice().createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });
					auto [result5, fence_] = VulkanEngine::GetLogicalDevice().createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });

					CORI_CORE_ASSERT(result4 == vk::Result::eSuccess, "Failed to create fence. Error: {}", vk::to_string(result4));
					CORI_CORE_ASSERT(result5 == vk::Result::eSuccess, "Failed to create fence. Error: {}", vk::to_string(result5));

					VulkanEngine::SetDebugName(fence, std::format("VulkanUploadManager HighPriority Fence {}", i));
					VulkanEngine::SetDebugName(fence_, std::format("VulkanUploadManager LowPriority Fence {}", i));

					m_HighPriorityFences[i] = fence;
					m_LowPriorityFences[i] = fence_;

					m_HighPriorityPrimaryCommandBuffers[i] = pCmb[i];
					m_LowPriorityPrimaryCommandBuffers[i] = pCmb[i + FRAMES_IN_FLIGHT];
					m_AmazingBuffersPrimaryCommandBuffers[i] = pCmb[i + FRAMES_IN_FLIGHT * 2];

					m_HighPrioritySecondaryCommandBuffers[i] = sCmb[i];
					m_LowPrioritySecondaryCommandBuffers[i] = sCmb[i + FRAMES_IN_FLIGHT];

					VulkanEngine::SetDebugName(m_HighPriorityPrimaryCommandBuffers[i], std::format("VulkanUploadManager HighPriority Primary Command Buffer {}", i));
					VulkanEngine::SetDebugName(m_LowPriorityPrimaryCommandBuffers[i], std::format("VulkanUploadManager LowPriority Primary Command Buffer {}", i));
					VulkanEngine::SetDebugName(m_AmazingBuffersPrimaryCommandBuffers[i], std::format("VulkanUploadManager Amazing Buffers Primary Command Buffer {}", i));

					VulkanEngine::SetDebugName(m_HighPrioritySecondaryCommandBuffers[i], std::format("VulkanUploadManager HighPriority Secondary Command Buffer {}", i));
					VulkanEngine::SetDebugName(m_LowPrioritySecondaryCommandBuffers[i], std::format("VulkanUploadManager LowPriority Secondary Command Buffer {}", i));
				}



				auto [result4, semaphore] = VulkanEngine::GetLogicalDevice().createSemaphore(vk::SemaphoreCreateInfo());
				auto [result5, fence] = VulkanEngine::GetLogicalDevice().createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });

				CORI_CORE_ASSERT(result4 == vk::Result::eSuccess, "Failed to create semaphore. Error: {}", vk::to_string(result4));
				CORI_CORE_ASSERT(result5 == vk::Result::eSuccess, "Failed to create fence. Error: {}", vk::to_string(result5));

				m_AmazingSemaphore = semaphore;
				m_AmazingBuffersFence = fence;

				VulkanEngine::SetDebugName(m_AmazingSemaphore, "VulkanUploadManager amazing buffer semaphore.");
				VulkanEngine::SetDebugName(m_AmazingBuffersFence, "VulkanUploadManager amazing buffer fence.");

				#if 0
				vk::SemaphoreTypeCreateInfo semType {
					.semaphoreType = vk::SemaphoreType::eTimeline,
					.initialValue = 0
				};

				vk::SemaphoreCreateInfo semInfo {
					.pNext = &semType,
				};

				auto [result_, semaphore] = VulkanEngine::GetLogicalDevice().createSemaphore(semInfo);

				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create timeline semaphore. Error: {}", vk::to_string(result_));

				m_TimelineSemaphore = semaphore;
				VulkanEngine::SetDebugName(m_TimelineSemaphore, "VulkanUploadManager Timeline Semaphore");
				#endif

				m_HighPriorityHeapSize = HIGH_PRIORITY_STAGING_SIZE;
				m_LowPriorityHeapSize = LOW_PRIORITY_STAGING_SIZE;

				vk::BufferCreateInfo highPriorityCreateInfo {
					.size = 1024 * 1024 * m_HighPriorityHeapSize,
					.usage = vk::BufferUsageFlagBits::eTransferSrc,
					.sharingMode = vk::SharingMode::eExclusive
				};

				vk::BufferCreateInfo lowPriorityCreateInfo {
					.size = 1024 * 1024 * m_LowPriorityHeapSize,
					.usage = vk::BufferUsageFlagBits::eTransferSrc,
					.sharingMode = vk::SharingMode::eExclusive
				};

				vma::AllocationCreateInfo allocCreateInfo {
					.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
					.usage = vma::MemoryUsage::eAuto
				};

				VulkanBuffer::CreateInfo highPriorityInfo {
					.bufferCreateInfo = &highPriorityCreateInfo,
					.allocationCreateInfo = &allocCreateInfo,
				};

				VulkanBuffer::CreateInfo lowPriorityInfo {
					.bufferCreateInfo = &lowPriorityCreateInfo,
					.allocationCreateInfo = &allocCreateInfo,
				};

				m_HighPriorityRingStagingBuffer = VulkanBuffer::Create(highPriorityInfo);
				m_LowPriorityRingStagingBuffer = VulkanBuffer::Create(lowPriorityInfo);

				vma::VirtualBlockCreateInfo highPriorityBlockInfo {
					.size = 1024 * 1024 * m_HighPriorityHeapSize,
					.flags = vma::VirtualBlockCreateFlagBits::eLinearAlgorithm
				};

				vma::VirtualBlockCreateInfo lowPriorityBlockInfo {
					.size = 1024 * 1024 * m_LowPriorityHeapSize,
					.flags = vma::VirtualBlockCreateFlagBits::eLinearAlgorithm
				};

				auto [result2, highPriorityBlock] = vma::createVirtualBlock(highPriorityBlockInfo);
				auto [result3, lowPriorityBlock] = vma::createVirtualBlock(lowPriorityBlockInfo);

				CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "Failed to create vma high priority virtual block. Error: {}", vk::to_string(result2));
				CORI_CORE_ASSERT(result3 == vk::Result::eSuccess, "Failed to create vma low priority virtual block. Error: {}", vk::to_string(result3));

				m_HighPriorityBlock = highPriorityBlock;
				m_LowPriorityBlock = lowPriorityBlock;

				m_TempAllocs.reserve(6);
				m_HighPriorityUploads.reserve(64);
				m_BufferBarriersCache.reserve(64);
				m_ImageBarriersCache.reserve(64);
			}

			void Reclaim() {
				while (!m_PendingHighPriorityUploads.empty()) {
					auto& [alloc, frameConsumer] = m_PendingHighPriorityUploads.front();

					vk::Result status = VulkanEngine::GetLogicalDevice().getFenceStatus(m_HighPriorityFences[frameConsumer]);

					if (status == vk::Result::eNotReady) {
						break;
					}

					m_HighPriorityBlock.free(alloc);
					m_PendingHighPriorityUploads.pop();
				}

				while (!m_PendingLowPriorityUploads.empty()) {
					auto& [alloc, frameConsumer] = m_PendingLowPriorityUploads.front();

					vk::Result status = VulkanEngine::GetLogicalDevice().getFenceStatus(m_LowPriorityFences[frameConsumer]);

					if (status == vk::Result::eNotReady) {
						break;
					}

					m_LowPriorityBlock.free(alloc);
					m_PendingLowPriorityUploads.pop();
				}
			}

			struct Allocation {
				vk::DeviceSize offset{};
				vma::VirtualAllocation virtualAllocation;
			};

			struct PendingUpload {
				vma::VirtualAllocation allocation;
				uint32_t consumerFrame{ UINT32_MAX };
			};

			std::optional<Allocation> AllocateHighPriority(uint64_t size, uint64_t alignment) {
				if (size > 1024 * 1024 * m_HighPriorityHeapSize) {
					return std::nullopt;
				}

				vma::VirtualAllocationCreateInfo allocInfo {
					.size = size,
					.alignment = alignment,
					.flags = vma::VirtualAllocationCreateFlagBits::eStrategyMinTime
				};

				vk::DeviceSize offset;
				auto [result, virtAlloc] = m_HighPriorityBlock.virtualAllocate(allocInfo, offset);
				if (result != vk::Result::eSuccess) {
					return std::nullopt;
				}

				Allocation allocation;
				//allocation.ptr = static_cast<uint8_t*>(m_MappedHighPriorityBufferPtr) + offset;
				allocation.offset = offset;
				allocation.virtualAllocation = virtAlloc;
				return allocation;
			}

			std::optional<Allocation> AllocateLowPriority(uint64_t size, uint64_t alignment) {
				if (size > 1024 * 1024 * m_LowPriorityHeapSize) {
					return std::nullopt;
				}

				vma::VirtualAllocationCreateInfo allocInfo {
					.size = size,
					.alignment = alignment,
					.flags = vma::VirtualAllocationCreateFlagBits::eStrategyMinTime
				};

				vk::DeviceSize offset;
				auto [result, virtAlloc] = m_LowPriorityBlock.virtualAllocate(allocInfo, offset);
				if (result != vk::Result::eSuccess) {
					return std::nullopt;
				}

				Allocation allocation;
				//allocation.ptr = static_cast<uint8_t*>(m_MappedLowPriorityBufferPtr) + offset;
				allocation.offset = offset;
				allocation.virtualAllocation = virtAlloc;
				return allocation;
			}



			VulkanBuffer m_HighPriorityRingStagingBuffer;
			VulkanBuffer m_LowPriorityRingStagingBuffer;

			vma::VirtualBlock m_HighPriorityBlock;
			vma::VirtualBlock m_LowPriorityBlock;

			std::vector<Allocation> m_TempAllocs;

			std::vector<UploadRequest> m_HighPriorityUploads;
			std::queue<UploadRequest> m_LowPriorityUploads;

			std::queue<PendingUpload> m_PendingHighPriorityUploads;
			std::queue<PendingUpload> m_PendingLowPriorityUploads;

			std::array<vk::Fence, FRAMES_IN_FLIGHT> m_HighPriorityFences;
			std::array<vk::Fence, FRAMES_IN_FLIGHT> m_LowPriorityFences;

			vk::Fence m_AmazingBuffersFence;
			vk::Semaphore m_AmazingSemaphore;

			std::array<vk::Semaphore, FRAMES_IN_FLIGHT> m_HighPrioritySubmitSemaphores;
			std::array<vk::Semaphore, FRAMES_IN_FLIGHT> m_LowPrioritySubmitSemaphores;

			std::array<vk::CommandBuffer, FRAMES_IN_FLIGHT> m_HighPriorityPrimaryCommandBuffers;
			std::array<vk::CommandBuffer, FRAMES_IN_FLIGHT> m_LowPriorityPrimaryCommandBuffers;
			std::array<vk::CommandBuffer, FRAMES_IN_FLIGHT> m_AmazingBuffersPrimaryCommandBuffers;

			std::array<vk::CommandBuffer, FRAMES_IN_FLIGHT> m_HighPrioritySecondaryCommandBuffers;
			std::array<vk::CommandBuffer, FRAMES_IN_FLIGHT> m_LowPrioritySecondaryCommandBuffers;

			std::vector<vk::BufferMemoryBarrier2> m_BufferBarriersCache;
			std::vector<vk::ImageMemoryBarrier2> m_ImageBarriersCache;

			std::vector<AmazingBuffer> m_AmazingBuffers;
			std::vector<AmazingBufferHandle> m_Holes;

			//vk::Semaphore m_TimelineSemaphore;

			uint64_t m_HighPriorityHeapSize{ 0 };
			uint64_t m_LowPriorityHeapSize{ 0 };

			//uint64_t m_NextTimelineValue{ 1 };

			static std::unique_ptr<VulkanUploadManager> s_Instance;
		};
	}
}
