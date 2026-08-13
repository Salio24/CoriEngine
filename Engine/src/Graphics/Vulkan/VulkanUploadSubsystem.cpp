#include "VulkanUploadSubsystem.hpp"


namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanVirtualBufferAllocator> VulkanVirtualBufferAllocator::s_Instance{ nullptr };

		void VulkanVirtualBufferAllocator::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanVirtualBufferAllocator is already initialized.");
			s_Instance = std::unique_ptr<VulkanVirtualBufferAllocator>(new VulkanVirtualBufferAllocator());
		}

		void VulkanVirtualBufferAllocator::Shutdown() {
			s_Instance.reset();
		}

		VulkanVirtualBufferAllocator& VulkanVirtualBufferAllocator::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanVirtualBufferAllocator::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		VulkanVirtualBufferAllocator::VulkanVirtualBufferAllocator() {
			vk::BufferCreateInfo arenaBufferInfo {
				.size = UPLOAD_ARENA_SIZE,
				.usage = HEAP_USAGE_FLAGS,
				.sharingMode = vk::SharingMode::eExclusive
			};

			vma::AllocationCreateInfo arenaAllocInfo {
				.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead | vma::AllocationCreateFlagBits::eMapped,
				.usage = vma::MemoryUsage::eAuto,
				.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
			};

			VulkanBuffer::CreateInfo arenaCreateInfo {
				.bufferCreateInfo = &arenaBufferInfo,
				.allocationCreateInfo = &arenaAllocInfo,
				.name = "Virtual Buffer Allocator's GPU side arena heap"
			};

			VulkanBuffer arena = VulkanBuffer::Create(arenaCreateInfo);

			vk::MemoryPropertyFlags arenaMemoryFlags = VulkanEngine::GetAllocator().getAllocationMemoryProperties(arena.m_Allocation);
			if (arenaMemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible) {
				m_UploadArenaType = UploadArenaType::BAR;
				m_UploadArenaHeap = arena;
			} else {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBufferAllocator }, "Failed to allocate frame upload arena on BAR, using staging buffer fallback, there might be a very slight performance hit.");

				vma::AllocationCreateInfo cpuArenaAllocInfo {
					.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
					.usage = vma::MemoryUsage::eAuto
				};

				VulkanBuffer::CreateInfo cpuArenaCreateInfo {
					.bufferCreateInfo = &arenaBufferInfo,
					.allocationCreateInfo = &cpuArenaAllocInfo,
					.name = "Virtual Buffer Allocator's CPU side arena heap"
				};

				VulkanBuffer cpuArena = VulkanBuffer::Create(cpuArenaCreateInfo);
				m_UploadArenaHeap = std::make_pair(arena, cpuArena);

				m_UploadArenaType = UploadArenaType::STAGING;

				VulkanVirtualBuffer::s_UploadListener = std::bind(&VulkanVirtualBufferAllocator::ProcessUpload, this, std::placeholders::_1);

				m_CopyRegions.reserve(32);
			}
			m_UploadArenaBDA = arena.GetBDA();

			vma::VirtualBlockCreateInfo uploadArenaVirtualBlockCreateInfo {
				.size = UPLOAD_ARENA_SIZE,
				.flags = vma::VirtualBlockCreateFlagBits::eLinearAlgorithm
			};

			auto [result, vBlock] = vma::createVirtualBlock(uploadArenaVirtualBlockCreateInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create VulkanVirtualBufferAllocator's upload arena vma virtual block. Error: {}", vk::to_string(result));
			m_UploadArenaBlock = vBlock;

			vk::BufferCreateInfo scratchBufferCreateInfo {
				.size = GPU_SCRATCH_SIZE,
				.usage = HEAP_USAGE_FLAGS,
				.sharingMode = vk::SharingMode::eExclusive
			};

			vma::AllocationCreateInfo scratchAllocationCreateInfo {
				.flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
				.usage = vma::MemoryUsage::eAuto
			};

			VulkanBuffer::CreateInfo scratchBufferInfo {
				.bufferCreateInfo = &scratchBufferCreateInfo,
				.allocationCreateInfo = &scratchAllocationCreateInfo,
				.name = "Virtual Buffer Allocator's GPU scratch heap"
			};

			m_GPUScratchHeap = VulkanBuffer::Create(scratchBufferInfo);

			m_GPUScratchBDA = m_GPUScratchHeap.GetBDA();

			vma::VirtualBlockCreateInfo scratchVirtualBlockCreateInfo {
				.size = GPU_SCRATCH_SIZE,
				.flags = vma::VirtualBlockCreateFlagBits::eLinearAlgorithm
			};

			auto [result_, vBlock_] = vma::createVirtualBlock(scratchVirtualBlockCreateInfo);
			CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create VulkanVirtualBufferAllocator's GPU scratch vma virtual block. Error: {}", vk::to_string(result_));
			m_GPUScratchBlock = vBlock_;

			for (auto& vec : m_GPUScratchVirtualAllocations) {
				vec.reserve(16);
			}

			for (auto& vec : m_UploadVirtualAllocations) {
				vec.reserve(16);
			}
		}

		VulkanVirtualBufferAllocator::~VulkanVirtualBufferAllocator() {
			DeletionQueue::PushBuffer(m_GPUScratchHeap);

			if (std::holds_alternative<VulkanBuffer>(m_UploadArenaHeap)) {
				DeletionQueue::PushBuffer(std::get<VulkanBuffer>(m_UploadArenaHeap));
			} else {
				auto [gpuArena, cpuArena] = std::get<std::pair<VulkanBuffer, VulkanBuffer>>(m_UploadArenaHeap);
				DeletionQueue::PushBuffer(gpuArena);
				DeletionQueue::PushBuffer(cpuArena);
			}

			m_GPUScratchBlock.clearVirtualBlock();
			m_UploadArenaBlock.clearVirtualBlock();

			DeletionQueue::PushVirtualBlock(m_GPUScratchBlock);
			DeletionQueue::PushVirtualBlock(m_UploadArenaBlock);
		}

		VulkanVirtualBuffer VulkanVirtualBufferAllocator::CreateVirtualUploadBuffer(uint64_t size, const uint64_t alignment, const uint32_t dstFrameInFlight, const char* name) {
			CORI_PROFILE_FUNCTION();

			size = Math::AlignUp(size, alignment);

			vma::VirtualAllocationCreateInfo allocInfo {
				.size = size,
				.alignment = alignment
			};

			vk::DeviceSize offset;
			auto [result, alloc] = Get().m_UploadArenaBlock.virtualAllocate(allocInfo, &offset);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create VulkanVirtualBuffer from CPU upload arena heap, likely out of memory. Error: {}", vk::to_string(result));

			Get().m_UploadVirtualAllocations[dstFrameInFlight].emplace_back(alloc);

			VulkanVirtualBuffer virtBuff;
			virtBuff.m_StartOffset = offset;
			virtBuff.m_Size = size;
			if (std::holds_alternative<VulkanBuffer>(Get().m_UploadArenaHeap)) {
				auto heap = std::get<VulkanBuffer>(Get().m_UploadArenaHeap);
				virtBuff.m_Heap = heap.m_Buffer;
				virtBuff.m_Alloc = heap.m_Allocation;
			} else {
				auto cpuHeap = std::get<std::pair<VulkanBuffer, VulkanBuffer>>(Get().m_UploadArenaHeap).second;
				virtBuff.m_Heap = cpuHeap.m_Buffer;
				virtBuff.m_Alloc = cpuHeap.m_Allocation;
			}

			virtBuff.m_StartBDA = Get().m_UploadArenaBDA + virtBuff.m_StartOffset;
			virtBuff.m_Type = VulkanVirtualBuffer::Type::CPUUpload;
			#ifdef DEBUG_BUILD
			virtBuff.m_Name = name;
			#endif
			virtBuff.m_Alignment = alignment;
			return virtBuff;
		}

		VulkanVirtualBuffer VulkanVirtualBufferAllocator::CreateVirtualScratchBuffer(uint64_t size, const uint64_t alignment, const uint32_t dstFrameInFlight, const char* name) {
			CORI_PROFILE_FUNCTION();

			size = Math::AlignUp(size, alignment);

			vma::VirtualAllocationCreateInfo allocInfo {
				.size = size,
				.alignment = alignment
			};

			vk::DeviceSize offset;
			auto [result, alloc] = Get().m_GPUScratchBlock.virtualAllocate(allocInfo, &offset);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create VulkanVirtualBuffer from GPU scratch heap, likely out of memory. Error: {}", vk::to_string(result));

			Get().m_GPUScratchVirtualAllocations[dstFrameInFlight].emplace_back(alloc);

			VulkanVirtualBuffer virtBuff;
			virtBuff.m_StartOffset = offset;
			virtBuff.m_Size = size;
			virtBuff.m_Heap = Get().m_GPUScratchHeap.m_Buffer;
			virtBuff.m_StartBDA = Get().m_GPUScratchBDA + virtBuff.m_StartOffset;
			virtBuff.m_Type = VulkanVirtualBuffer::Type::GPUScratch;
			#ifdef DEBUG_BUILD
			virtBuff.m_Name = name;
			#endif
			virtBuff.m_Alignment = alignment;
			return virtBuff;
		}

		void VulkanVirtualBufferAllocator::ClearGPUScratchBlock(uint32_t frameInFlight) {
			for (auto& alloc : Get().m_GPUScratchVirtualAllocations[frameInFlight]) {
				Get().m_GPUScratchBlock.free(alloc);
			}

			Get().m_GPUScratchVirtualAllocations[frameInFlight].clear();

			for (auto& alloc : Get().m_UploadVirtualAllocations[frameInFlight]) {
				Get().m_UploadArenaBlock.free(alloc);
			}

			Get().m_UploadVirtualAllocations[frameInFlight].clear();
		}

		void VulkanVirtualBufferAllocator::SubmitCopies(vk::CommandBuffer cmb) {
			if (Get().m_UploadArenaType == UploadArenaType::STAGING) {
				CORI_VK_LABEL_F(cmb, DebugLabelColors::Upload, "Upload arena staging copies ({} region(s))", Get().m_CopyRegions.size());

				auto [gpu, cpu] = std::get<std::pair<VulkanBuffer, VulkanBuffer>>(Get().m_UploadArenaHeap);

				std::array<vk::BufferMemoryBarrier2, 2> startBarriers;

				startBarriers[0].srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
				startBarriers[0].srcAccessMask = vk::AccessFlagBits2::eNone;
				startBarriers[0].dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
				startBarriers[0].dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
				startBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				startBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				startBarriers[0].buffer = gpu.m_Buffer;
				startBarriers[0].offset = 0;
				startBarriers[0].size = VK_WHOLE_SIZE;

				startBarriers[1].srcStageMask = vk::PipelineStageFlagBits2::eHost;
				startBarriers[1].srcAccessMask = vk::AccessFlagBits2::eHostWrite;
				startBarriers[1].dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
				startBarriers[1].dstAccessMask = vk::AccessFlagBits2::eTransferRead;
				startBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				startBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				startBarriers[1].buffer = cpu.m_Buffer;
				startBarriers[1].offset = 0;
				startBarriers[1].size = VK_WHOLE_SIZE;

				vk::DependencyInfo depInfo1 {
					.bufferMemoryBarrierCount = static_cast<uint32_t>(startBarriers.size()),
					.pBufferMemoryBarriers = startBarriers.data()
				};

				cmb.pipelineBarrier2(depInfo1);

				vk::CopyBufferInfo2 copyInfo {
					.srcBuffer = cpu.m_Buffer,
					.dstBuffer = gpu.m_Buffer,
					.regionCount = static_cast<uint32_t>(Get().m_CopyRegions.size()),
					.pRegions = Get().m_CopyRegions.data()
				};

				cmb.copyBuffer2(copyInfo);

				std::array<vk::BufferMemoryBarrier2, 2> endBarriers;

				endBarriers[0].srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
				endBarriers[0].srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
				endBarriers[0].dstStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
				endBarriers[0].dstAccessMask = vk::AccessFlagBits2::eNone;
				endBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				endBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				endBarriers[0].buffer = gpu.m_Buffer;
				endBarriers[0].offset = 0;
				endBarriers[0].size = VK_WHOLE_SIZE;

				endBarriers[1].srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
				endBarriers[1].srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
				endBarriers[1].dstStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
				endBarriers[1].dstAccessMask = vk::AccessFlagBits2::eNone;
				endBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				endBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				endBarriers[1].buffer = cpu.m_Buffer;
				endBarriers[1].offset = 0;
				endBarriers[1].size = VK_WHOLE_SIZE;

				vk::DependencyInfo depInfo2 {
					.bufferMemoryBarrierCount = static_cast<uint32_t>(endBarriers.size()),
					.pBufferMemoryBarriers = endBarriers.data()
				};

				cmb.pipelineBarrier2(depInfo2);

				Get().m_CopyRegions.clear();
			}

		}

		void VulkanVirtualBufferAllocator::ProcessUpload(const VulkanVirtualBuffer& buffer) {
			m_CopyRegions.emplace_back(vk::BufferCopy2{
				.srcOffset = buffer.m_StartOffset,
				.dstOffset = buffer.m_StartOffset,
				.size = buffer.m_Size
			});
		}

		std::unique_ptr<VulkanDynamicContainerUploadManager> VulkanDynamicContainerUploadManager::s_Instance{ nullptr };

		void VulkanDynamicContainerUploadManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanDynamicContainerUploadManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanDynamicContainerUploadManager>(new VulkanDynamicContainerUploadManager());
		}

		void VulkanDynamicContainerUploadManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanDynamicContainerUploadManager& VulkanDynamicContainerUploadManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanDynamicContainerUploadManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		VulkanDynamicContainerUploadManager::~VulkanDynamicContainerUploadManager() {
			if (m_RingStagingBuffer.m_Buffer) {
				DeletionQueue::PushBuffer(m_RingStagingBuffer);
			}

			if (m_RingStagingBlock) {
				DeletionQueue::PushVirtualBlock(m_RingStagingBlock);
			}
		}

		void VulkanDynamicContainerUploadManager::ProcessUpdates(vk::CommandBuffer cmb) {
			if (!Get().m_Initialized) {
				return;
			}

			for (auto& pendingCopy : Get().m_PendingCopies) {
				Get().m_BufferMemoryBarrierCache.emplace_back(vk::BufferMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					.srcAccessMask = vk::AccessFlagBits2::eNone,
					.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = pendingCopy.srcBuffer,
					.offset = 0,
					.size = VK_WHOLE_SIZE
				});
			}

			if (!Get().m_BufferMemoryBarrierCache.empty()) {
				vk::DependencyInfo depInfo {
					.bufferMemoryBarrierCount = static_cast<uint32_t>(Get().m_BufferMemoryBarrierCache.size()),
					.pBufferMemoryBarriers = Get().m_BufferMemoryBarrierCache.data()
				};

				cmb.pipelineBarrier2(depInfo);
			}

			Get().m_BufferMemoryBarrierCache.clear();

			for (auto& pendingCopy : Get().m_PendingCopies) {
				vk::BufferCopy2 region {
					.srcOffset = 0,
					.dstOffset = 0,
					.size = pendingCopy.size
				};

				vk::CopyBufferInfo2	info {
					.srcBuffer = pendingCopy.srcBuffer,
					.dstBuffer = pendingCopy.dstBuffer,
					.regionCount = 1,
					.pRegions = &region
				};

				cmb.copyBuffer2(&info);

				Get().m_BufferMemoryBarrierCache.emplace_back(vk::BufferMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = pendingCopy.dstBuffer,
					.offset = 0,
					.size = VK_WHOLE_SIZE
				});
			}
			if (!Get().m_BufferMemoryBarrierCache.empty()) {
				vk::DependencyInfo depInfo = {
					.bufferMemoryBarrierCount = static_cast<uint32_t>(Get().m_BufferMemoryBarrierCache.size()),
					.pBufferMemoryBarriers = Get().m_BufferMemoryBarrierCache.data()
				};

				cmb.pipelineBarrier2(depInfo);
			}

			Get().m_BufferMemoryBarrierCache.clear();

			Get().m_PendingCopies.clear();

			for (auto& bufferCopyInfo : Get().m_BufferCopyInfos) {
				vk::CopyBufferInfo2 info {
					.srcBuffer = bufferCopyInfo.srcBuffer,
					.dstBuffer = bufferCopyInfo.dstBuffer,
					.regionCount = bufferCopyInfo.regionCount,
					.pRegions = Get().m_CopyRegions.data() + bufferCopyInfo.firstRegion
				};

				cmb.copyBuffer2(&info);

				Get().m_BufferMemoryBarrierCache.emplace_back(vk::BufferMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.dstStageMask = s_GenericReadStages,
					.dstAccessMask = s_GenericReadAccess,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = bufferCopyInfo.dstBuffer,
					.offset = 0,
					.size = VK_WHOLE_SIZE
				});
			}
			if (!Get().m_BufferMemoryBarrierCache.empty()) {
				vk::DependencyInfo depInfo = {
					.bufferMemoryBarrierCount = static_cast<uint32_t>(Get().m_BufferMemoryBarrierCache.size()),
					.pBufferMemoryBarriers = Get().m_BufferMemoryBarrierCache.data()
				};

				cmb.pipelineBarrier2(depInfo);
			}

			Get().m_BufferMemoryBarrierCache.clear();
			Get().m_BufferCopyInfos.clear();
			Get().m_CopyRegions.clear();
		}

		void VulkanDynamicContainerUploadManager::BeginUpdate(vk::Buffer dstBuffer) {
			Get().m_UploaderMutex.lock();

			auto& info = Get().m_BufferCopyInfos.emplace_back();

			info.dstBuffer = dstBuffer;
			info.srcBuffer = Get().m_RingStagingBuffer.m_Buffer;
			info.regionCount = 0;
			info.firstRegion = Get().m_CopyRegions.size();

		}

		void VulkanDynamicContainerUploadManager::Upload(const void* data, const uint64_t size, const uint64_t offset) {
			CORI_CORE_ASSERT(Get().m_Initialized, "Upload is called on non initialized VulkanDynamicContainerUploadManager.");

			vma::VirtualAllocationCreateInfo allocInfo {
				.size = size,
				.alignment = 4,
				.flags = vma::VirtualAllocationCreateFlagBits::eStrategyMinTime
			};

			vk::DeviceSize stageOffset;
			auto [result, virtAlloc] = Get().m_RingStagingBlock.virtualAllocate(allocInfo, stageOffset);

			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to allocate from staging buffer of VulkanDynamicContainerUploadManager. Error: {}", vk::to_string(result));

			result = VulkanEngine::GetAllocator().copyMemoryToAllocation(data, Get().m_RingStagingBuffer.m_Allocation, stageOffset, size);

			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy to staging buffer of VulkanDynamicContainerUploadManager. Error: {}", vk::to_string(result));


			auto& region = Get().m_CopyRegions.emplace_back();
			region.srcOffset = stageOffset;
			region.dstOffset = offset;
			region.size = size;

			auto& info = Get().m_BufferCopyInfos.back();
			info.regionCount++;

			DeletionQueue::PushVirtualAlloc(virtAlloc, Get().m_RingStagingBlock);
		}

		void VulkanDynamicContainerUploadManager::EndUpdate() {
			Get().m_UploaderMutex.unlock();
		}

		void VulkanDynamicContainerUploadManager::RecordCopy(VulkanBuffer& srcBuffer, vk::Buffer dstBuffer, const uint64_t size) {
			CORI_CORE_ASSERT(Get().m_Initialized, "RecordCopy is called on non initialized VulkanDynamicContainerUploadManager.");
			Get().m_UploaderMutex.lock();

			Get().m_PendingCopies.emplace_back(srcBuffer.m_Buffer, dstBuffer, size);

			DeletionQueue::PushBuffer(srcBuffer);

			Get().m_UploaderMutex.unlock();
		}

		void VulkanDynamicContainerUploadManager::FallbackListener() {
			Get().m_UploaderMutex.lock();
			if (!Get().m_Initialized) {
				vma::AllocationCreateInfo allocationCreateInfo {
					.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
					.usage = vma::MemoryUsage::eAuto
				};

				vk::BufferCreateInfo bufferCreateInfo {
					.size = STAGING_SIZE,
					.usage = vk::BufferUsageFlagBits::eTransferSrc,
					.sharingMode = vk::SharingMode::eExclusive
				};

				VulkanBuffer::CreateInfo info {
					.bufferCreateInfo = &bufferCreateInfo,
					.allocationCreateInfo = &allocationCreateInfo,
					.name = "VulkanDynamicContainerUploadManager staging buffer"
				};

				Get().m_RingStagingBuffer = VulkanBuffer::Create(info);

				vma::VirtualBlockCreateInfo blockInfo {
					.size = STAGING_SIZE,
					.flags = vma::VirtualBlockCreateFlagBits::eLinearAlgorithm
				};

				auto [result, virtBlock] = vma::createVirtualBlock(blockInfo);

				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create vma low virtual block for VulkanDynamicContainerUploadManager. Error: {}", vk::to_string(result));

				Get().m_RingStagingBlock = virtBlock;

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					Get().m_DestructionQueue[i].reserve(64);
				}

				Get().m_BufferCopyInfos.reserve(8);
				Get().m_CopyRegions.reserve(64);
				Get().m_BufferMemoryBarrierCache.reserve(256);

				Get().m_Initialized = true;
			}
			Get().m_UploaderMutex.unlock();
		}

		std::unique_ptr<VulkanStreamingLine> VulkanStreamingLine::s_Instance{ nullptr };

		void VulkanStreamingLine::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanStreamingLine is already initialized.");
			s_Instance = std::unique_ptr<VulkanStreamingLine>(new VulkanStreamingLine());
		}

		void VulkanStreamingLine::Shutdown() {
			s_Instance.reset();
		}

		VulkanStreamingLine& VulkanStreamingLine::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanStreamingLine::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		std::expected<uint64_t, ErrorCode> VulkanStreamingLine::SubmitUploads(const std::span<const GenericUpload>& uploads) {
			CORI_PROFILE_FUNCTION();
			auto freeSlot = Get().GetFreeTransferSlot();
			if (!freeSlot) {
				return std::unexpected(ErrorCode::eNotReady);
			}
			auto& slot = Get().m_Slots[freeSlot.value()];
			uint32_t startOffset = slot.pendingUploads.size();
			if (slot.pendingUploads.capacity() < startOffset + uploads.size()) {
				slot.pendingUploads.reserve(slot.pendingUploads.capacity() >= 4 ? slot.pendingUploads.capacity() * 1.5f : 4);
			}

			bool success = true;
			bool outOfMemory = false;

			{
				CORI_PROFILE_SCOPE("Loop");
				for (auto [upload, data] : uploads) {
					if (std::holds_alternative<std::monostate>(upload) || data.size_bytes() == 0) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "At least one GenericUpload provided to SubmitUploads is in an invalid state, no upload will be made, this batch will be skipped.");
						success = false;
						break;
					}

					if (std::holds_alternative<BufferUpload>(upload)) {
						auto& bufferUpload = std::get<BufferUpload>(upload);

						if (bufferUpload.range.offset % bufferUpload.range.alignment != 0) {
						#ifdef DEBUG_BUILD
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset '{}' provided when calling SubmitUpdate for resource VulkanBuffer '{}' is misaligned to the provided alignment of '{}', no upload will be made, this batch will be skipped.", bufferUpload.range.offset, bufferUpload.resource.GetName(), bufferUpload.range.alignment);
						#else
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Offset '{}' provided when calling SubmitUpdate for resource VulkanBuffer '{}' is misaligned to the provided alignment of '{}', no upload will be made, this batch will be skipped.", bufferUpload.range.offset, "Name is unavailable in release build", bufferUpload.range.alignment);
						#endif
							success = false;
							break;
						}

						if (bufferUpload.range.offset + data.size_bytes() > bufferUpload.resource.m_Size) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Trying to upload to VulkanBuffer '{}' via VulkanStreamingLine, but upload is out of bounds, offest '{}', data size '{}', buffer size '{}', no upload will be made, this batch will be skipped.", bufferUpload.resource.GetName(), bufferUpload.range.offset, data.size_bytes(), bufferUpload.resource.m_Size);
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Trying to upload to VulkanBuffer '{}' via VulkanStreamingLine, but upload is out of bounds, offest '{}', data size '{}', buffer size '{}', no upload will be made, this batch will be skipped.", "Name is unavailable in release build", bufferUpload.range.offset, data.size_bytes(), bufferUpload.resource.m_Size);
						#endif
							success = false;
							break;
						}

						auto alloc = Get().TryAllocate(data.size_bytes(), std::max<uint64_t>(4, bufferUpload.range.alignment));
						if (alloc) {
							slot.pendingUploads.emplace_back(bufferUpload, data.size_bytes(), alloc.value());
						} else {
							outOfMemory = true;
							break;
						}
					} else {
						auto& imageUpload = std::get<ImageUpload>(upload);

						if (imageUpload.range.subresourceLayers.baseArrayLayer + imageUpload.range.subresourceLayers.layerCount > imageUpload.resource.m_ArrayLayers) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid BaseArrayLayer '{}' and/or LayerCount '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, the sum of them can't be more than the total image layer count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.baseArrayLayer, imageUpload.range.subresourceLayers.layerCount, imageUpload.resource.GetName(), imageUpload.resource.m_ArrayLayers);
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid BaseArrayLayer '{}' and/or LayerCount '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, the sum of them can't be more than the total image layer count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.baseArrayLayer, imageUpload.range.subresourceLayers.layerCount, "Name is unavailable in release build", imageUpload.resource.m_ArrayLayers);
						#endif

							success = false;
							break;
						}

						if (imageUpload.range.subresourceLayers.mipLevel > imageUpload.resource.m_MipLevels) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid MipLevel '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, MipLevel can't be more than the total image mip level count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.mipLevel, imageUpload.resource.GetName(), imageUpload.resource.m_MipLevels);
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid MipLevel '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, MipLevel can't be more than the total image mip level count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.mipLevel, "Name is unavailable in release build", imageUpload.resource.m_MipLevels);
						#endif

							success = false;
							break;
						}

						if (imageUpload.range.extent.width == 0 || imageUpload.range.extent.height == 0 || imageUpload.range.extent.depth == 0) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid Extent3D '{} {} {}' provided when trying to upload to VulkanImage '{}' via streaming line, no extent part can be 0, no upload will be made, this batch will be skipped.", imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.resource.GetName());
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid Extent3D '{} {} {}' provided when trying to upload to VulkanImage '{}' via streaming line, no extent part can be 0, no upload will be made, this batch will be skipped.", imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, "Name is unavailable in release build");
						#endif

							success = false;
							break;
						}

						std::array<uint8_t, 3> blockExtent = vk::blockExtent(imageUpload.resource.m_Format);

						vk::Extent3D mipExtent{ std::max(1u, imageUpload.resource.m_Extent3D.width >> imageUpload.range.subresourceLayers.mipLevel), std::max(1u, imageUpload.resource.m_Extent3D.height >> imageUpload.range.subresourceLayers.mipLevel), std::max(1u, imageUpload.resource.m_Extent3D.depth >> imageUpload.range.subresourceLayers.mipLevel) };

						if (imageUpload.range.offset.x % blockExtent[0] != 0 || imageUpload.range.offset.y % blockExtent[1] != 0 || imageUpload.range.offset.z % blockExtent[2] != 0) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset3D '{} {} {}' provided when calling SubmitUpdate for resource VulkanImage '{}' is misaligned to the block extent '{} {} {}' of image format '{}', no upload will be made, this batch will be skipped.", imageUpload.range.offset.x, imageUpload.range.offset.y, imageUpload.range.offset.z, imageUpload.resource.GetName(), blockExtent[0], blockExtent[1], blockExtent[2], vk::to_string(imageUpload.resource.m_Format));
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset3D '{} {} {}' provided when calling SubmitUpdate for resource VulkanImage '{}' is misaligned to the block extend '{} {} {}' of image format '{}', no upload will be made, this batch will be skipped.", imageUpload.range.offset.x, imageUpload.range.offset.y, imageUpload.range.offset.z, "Name is unavailable in release build", blockExtent[0], blockExtent[1], blockExtent[2], vk::to_string(imageUpload.resource.m_Format));
						#endif

							success = false;
							break;
						}

						if (imageUpload.range.offset.x + imageUpload.range.extent.width > mipExtent.width || imageUpload.range.offset.y + imageUpload.range.extent.height > mipExtent.height || imageUpload.range.offset.z + imageUpload.range.extent.depth > mipExtent.depth) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset3D '{} {} {}' + Extent3D '{} {} {}' provided when calling SubmitUpdate for resource VulkanImage '{}' is located beyond the image mip level '{}' Extent3D '{} {} {}', no upload will be made, this batch will be skipped.", imageUpload.range.offset.x, imageUpload.range.offset.y, imageUpload.range.offset.z, imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.resource.GetName(), imageUpload.range.subresourceLayers.mipLevel, mipExtent.width, mipExtent.height, mipExtent.depth);
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset3D '{} {} {}' + Extent3D '{} {} {}' provided when calling SubmitUpdate for resource VulkanImage '{}' is located beyond the image mip level '{}' Extent3D '{} {} {}', no upload will be made, this batch will be skipped.", imageUpload.range.offset.x, imageUpload.range.offset.y, imageUpload.range.offset.z, imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, "Name is unavailable in release build", imageUpload.range.subresourceLayers.mipLevel, mipExtent.width, mipExtent.height, mipExtent.depth);
						#endif

							success = false;
							break;
						}

						if ((imageUpload.range.extent.width % blockExtent[0] != 0 && imageUpload.range.offset.x + imageUpload.range.extent.width != mipExtent.width) ||
							(imageUpload.range.extent.height % blockExtent[1] != 0 && imageUpload.range.offset.y + imageUpload.range.extent.height != mipExtent.height) ||
							(imageUpload.range.extent.depth % blockExtent[2] != 0 && imageUpload.range.offset.z + imageUpload.range.extent.depth != mipExtent.depth)) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid Extent3D '{} {} {}' provided when trying to upload to VulkanImage '{}' via StreamingLine, extent + offset is not at the image border and extent is not aligned to block extent '{} {} {}' of image format '{}', no upload will be made, this batch will be skipped.", imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.resource.GetName(), blockExtent[0], blockExtent[1], blockExtent[2], vk::to_string(imageUpload.resource.m_Format));
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid Extent3D '{} {} {}' provided when trying to upload to VulkanImage '{}' via StreamingLine, extent + offset is not at the image border and extent is not aligned to block extent '{} {} {}' of image format '{}', no upload will be made, this batch will be skipped.", imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, "Name is unavailable in release build", blockExtent[0], blockExtent[1], blockExtent[2], vk::to_string(imageUpload.resource.m_Format));
						#endif

							success = false;
							break;
							}

						uint8_t blockSize = vk::blockSize(imageUpload.resource.m_Format);
						uint64_t expectedDataSize = blockSize * std::ceil(static_cast<float>(imageUpload.range.extent.width) / static_cast<float>(blockExtent[0])) * std::ceil(static_cast<float>(imageUpload.range.extent.height) / static_cast<float>(blockExtent[1])) * std::ceil(static_cast<float>(imageUpload.range.extent.depth) / static_cast<float>(blockExtent[2])) * imageUpload.range.subresourceLayers.layerCount;

						if (data.size_bytes() != expectedDataSize) {
						#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Data with invalid size was provided when trying to upload to VulkanImage '{}' via StreamingLine, upload Extent3D '{} {} {}', mip layer '{}', array layer count '{}', image format '{}', block size '{}', total expected data size '{}', provided data size '{}', no upload will be made, this batch will be skipped.", imageUpload.resource.GetName(), imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.range.subresourceLayers.mipLevel, imageUpload.range.subresourceLayers.layerCount, vk::to_string(imageUpload.resource.m_Format), blockSize, expectedDataSize, data.size_bytes());
						#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Data with invalid size was provided when trying to upload to VulkanImage '{}' via StreamingLine, upload Extent3D '{} {} {}', mip layer '{}', array layer count '{}', image format '{}', block size '{}', total expected data size '{}', provided data size '{}', no upload will be made, this batch will be skipped.", "Name is unavailable in release build", imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.range.subresourceLayers.mipLevel, imageUpload.range.subresourceLayers.layerCount, vk::to_string(imageUpload.resource.m_Format), blockSize, expectedDataSize, data.size_bytes());
						#endif

							success = false;
							break;
						}


						auto alloc = Get().TryAllocate(data.size_bytes(), std::max<uint64_t>(4, std::max<uint64_t>(4, vk::blockSize(imageUpload.resource.m_Format))));
						if (alloc) {
							slot.pendingUploads.emplace_back(imageUpload, data.size_bytes(), alloc.value());
						} else {
							outOfMemory = true;
							break;
						}

					}
				}
			}

			{
				CORI_PROFILE_SCOPE("Scrape");
				if (!success || outOfMemory) {
					for (auto& invalid : std::ranges::subrange(slot.pendingUploads.begin() + startOffset, slot.pendingUploads.end())) {
						Get().m_StagingBlock.free(invalid.stagingAllocation.virtualAllocation);
					}

					slot.pendingUploads.erase(slot.pendingUploads.begin() + startOffset, slot.pendingUploads.end());
					if (outOfMemory) {
						ProcessUploads();
						return std::unexpected(ErrorCode::eNotReady);
					}

					return std::unexpected(ErrorCode::eInvalidData);
				}
			}

			{
				CORI_PROFILE_SCOPE("Copy");
				for (auto [in, pending] : std::views::zip(uploads, std::ranges::subrange(slot.pendingUploads.begin() + startOffset, slot.pendingUploads.end()))) {
					auto result = VulkanEngine::GetAllocator().copyMemoryToAllocation(in.data.data(), Get().m_RingStagingBuffer.m_Allocation, pending.stagingAllocation.offset, pending.stagingSize);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy streaming data to the staging buffer. Error: {}", vk::to_string(result));
				}
			}

			if (startOffset == 0) {
				slot.completionTicket = Get().m_NextTicket++;
			}

			return slot.completionTicket;
		}

		bool VulkanStreamingLine::CheckTicket(const uint64_t ticket) {
			auto [result, currentValue] = VulkanEngine::GetLogicalDevice().getSemaphoreCounterValue(Get().m_TimelineSemaphore);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to get value of the timeline semaphore in the streaming line. Error: {}", vk::to_string(result));

			return currentValue >= ticket;
		}

		vk::Semaphore VulkanStreamingLine::GetTimelineSemaphoreHandle() {
			return Get().m_TimelineSemaphore;
		}

		uint64_t VulkanStreamingLine::GetTimelineValue() {
			auto [result, currentValue] = VulkanEngine::GetLogicalDevice().getSemaphoreCounterValue(Get().m_TimelineSemaphore);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to get value of the timeline semaphore in the streaming line. Error: {}", vk::to_string(result));

			return currentValue;
		}

		std::expected<uint64_t, ErrorCode> VulkanStreamingLine::SubmitUploads(const GenericUpload& upload) {
			return SubmitUploads(std::span{ &upload, 1 });
		}

		void VulkanStreamingLine::ProcessUploads() {
			CORI_PROFILE_FUNCTION();

			uint64_t targetTicket = UINT64_MAX;
			for (const auto& slot : Get().m_Slots) {
				if (!slot.pendingUploads.empty() && slot.isBusy == false) {
					targetTicket = std::min(targetTicket, slot.completionTicket);
				}
			}

			for (auto& slot : Get().m_Slots) {
				if (!slot.pendingUploads.empty() && slot.isBusy == false && slot.completionTicket == targetTicket) {

					vk::CommandBufferInheritanceInfo inherit{
						.pNext = nullptr,
						.occlusionQueryEnable = vk::False
					};


					vk::CommandBufferBeginInfo secondaryBeginInfo {
						.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
						.pInheritanceInfo = &inherit,
					};

					auto result = slot.secondaryCmb.begin(secondaryBeginInfo);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin secondary command buffer recording in VulkanStreamingLine. Error: {}", vk::to_string(result));

					CORI_VK_LABEL_BEGIN_F(slot.secondaryCmb, DebugLabelColors::Transfer, "Streaming copies (ticket {}, {} upload(s))", slot.completionTicket, slot.pendingUploads.size());

					for (auto& pending : slot.pendingUploads) {
						CORI_PROFILE_GPU_ZONE_C(VulkanEngine::GetTransferGPUProfilerContext(), slot.secondaryCmb, "Streaming Copy", Cori::ProfileColors::GPUTransfer);
						CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Record streaming copy", Cori::ProfileColors::Upload);
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "%llu bytes", static_cast<unsigned long long>(pending.stagingSize));

						if (std::holds_alternative<ImageUpload>(pending.resourceUpload)) {
							auto& resourceUpload = std::get<ImageUpload>(pending.resourceUpload);

							CORI_VK_LABEL_INSERT_F(slot.secondaryCmb, DebugLabelColors::Transfer, "Image upload, {} bytes", pending.stagingSize);

							vk::BufferImageCopy region{
								.bufferOffset = pending.stagingAllocation.offset,
								.bufferRowLength = 0,
								.bufferImageHeight = 0,
								.imageSubresource = resourceUpload.range.subresourceLayers,
								.imageOffset = resourceUpload.range.offset,
								.imageExtent = resourceUpload.range.extent
							};

							vk::ImageSubresourceRange vkRange{
								.aspectMask = resourceUpload.range.subresourceLayers.aspectMask,
								.baseMipLevel = resourceUpload.range.subresourceLayers.mipLevel,
								.levelCount = 1,
								.baseArrayLayer = resourceUpload.range.subresourceLayers.baseArrayLayer,
								.layerCount = resourceUpload.range.subresourceLayers.layerCount
							};

							Get().m_AcquireImageBarriersCache.emplace_back(vk::ImageMemoryBarrier2{
								.srcStageMask = vk::PipelineStageFlagBits2::eNone,
								.srcAccessMask = vk::AccessFlagBits2::eNone,
								.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
								.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
								.oldLayout = vk::ImageLayout::eUndefined,
								.newLayout = vk::ImageLayout::eTransferDstOptimal,
								.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.image = resourceUpload.resource.m_Image,
								.subresourceRange = vkRange
							});

							slot.secondaryCmb.copyBufferToImage(Get().m_RingStagingBuffer.m_Buffer, resourceUpload.resource.m_Image, vk::ImageLayout::eTransferDstOptimal, region);

							Get().m_ReleaseImageBarriersCache.emplace_back(vk::ImageMemoryBarrier2{
								.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								.dstStageMask = vk::PipelineStageFlagBits2::eNone,
								.dstAccessMask = vk::AccessFlagBits2::eNone,
								.oldLayout = vk::ImageLayout::eTransferDstOptimal,
								.newLayout = resourceUpload.dstLayout,
								.srcQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex(),
								.dstQueueFamilyIndex = resourceUpload.dstQueueFamilyIndex,
								.image = resourceUpload.resource.m_Image,
								.subresourceRange = vkRange
							});

						} else {
							auto& resourceUpload = std::get<BufferUpload>(pending.resourceUpload);

							CORI_VK_LABEL_INSERT_F(slot.secondaryCmb, DebugLabelColors::Transfer, "Buffer upload, {} bytes", pending.stagingSize);

							Get().m_BufferBarriersCache.emplace_back(vk::BufferMemoryBarrier2{
								.srcStageMask = resourceUpload.srcPipelineStages,
								.srcAccessMask = resourceUpload.srcAccessFlags,
								.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
								.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
								.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.buffer = resourceUpload.resource.m_Buffer,
								.offset = resourceUpload.range.offset,
								.size = pending.stagingSize,
							});

							vk::BufferCopy region{
								.srcOffset = pending.stagingAllocation.offset,
								.dstOffset = resourceUpload.range.offset,
								.size = pending.stagingSize
							};

							slot.secondaryCmb.copyBuffer(Get().m_RingStagingBuffer.m_Buffer, resourceUpload.resource.m_Buffer, region);
						}
					}

					CORI_VK_LABEL_END(slot.secondaryCmb);

					result = slot.secondaryCmb.end();
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end secondary command buffer recording in VulkanStreamingLine. Error: {}", vk::to_string(result));

					vk::CommandBufferBeginInfo primaryBeginInfo {
						.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
					};

					result = slot.primaryCmb.begin(primaryBeginInfo);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin primary command buffer recording in VulkanStreamingLine. Error: {}", vk::to_string(result));

					CORI_VK_LABEL_BEGIN_F(slot.primaryCmb, DebugLabelColors::Transfer, "Streaming line ticket {}", slot.completionTicket);

					vk::DependencyInfo depInfoIn{};
					bool inEmpty = true;
					vk::DependencyInfo depInfoOut{};
					bool outEmpty = true;

					if (!Get().m_AcquireImageBarriersCache.empty()) {
						depInfoIn.imageMemoryBarrierCount = static_cast<uint32_t>(Get().m_AcquireImageBarriersCache.size());
						depInfoIn.pImageMemoryBarriers = Get().m_AcquireImageBarriersCache.data();

						depInfoOut.imageMemoryBarrierCount = static_cast<uint32_t>(Get().m_ReleaseImageBarriersCache.size());
						depInfoOut.pImageMemoryBarriers = Get().m_ReleaseImageBarriersCache.data();

						inEmpty = false;
						outEmpty = false;
					}

					if (!Get().m_BufferBarriersCache.empty()) {
						depInfoIn.bufferMemoryBarrierCount = static_cast<uint32_t>(Get().m_BufferBarriersCache.size());
						depInfoIn.pBufferMemoryBarriers = Get().m_BufferBarriersCache.data();

						inEmpty = false;
					}

					if (!inEmpty) {
						slot.primaryCmb.pipelineBarrier2(depInfoIn);
					}

					slot.primaryCmb.executeCommands(slot.secondaryCmb);

					if (!outEmpty) {
						slot.primaryCmb.pipelineBarrier2(depInfoOut);
					}

					CORI_VK_LABEL_END(slot.primaryCmb);

					result = slot.primaryCmb.end();
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end primary command buffer recording in VulkanStreamingLine. Error: {}", vk::to_string(result));

					vk::TimelineSemaphoreSubmitInfo timelineInfo {
						.signalSemaphoreValueCount = 1,
						.pSignalSemaphoreValues = &slot.completionTicket
					};

					vk::SubmitInfo submitInfo{
						.pNext = &timelineInfo,
						.commandBufferCount = 1,
						.pCommandBuffers = &slot.primaryCmb,
						.signalSemaphoreCount = 1,
						.pSignalSemaphores = &Get().m_TimelineSemaphore
					};

					CORI_VK_QUEUE_LABEL_INSERT_F(VulkanEngine::GetTransferQueue(), DebugLabelColors::Transfer, "Submit streaming line ticket {}", slot.completionTicket);

					result = VulkanEngine::GetTransferQueue().submit(1, &submitInfo, nullptr);
					VulkanDeviceLossDebug::CheckResult(result, "vkQueueSubmit on the transfer queue (VulkanStreamingLine)");
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Transfer queue submission failed in VulkanStreamingLine. Error: {}", vk::to_string(result));

					Get().m_BufferBarriersCache.clear();
					Get().m_AcquireImageBarriersCache.clear();
					Get().m_ReleaseImageBarriersCache.clear();

					slot.isBusy = true;
					break;
				}
			}
		}

		VulkanStreamingLine::~VulkanStreamingLine() {
			m_StagingBlock.clearVirtualBlock();
			m_StagingBlock.destroy();

			m_RingStagingBuffer.Destroy();

			for (auto& slot : m_Slots) {
				VulkanEngine::GetLogicalDevice().freeCommandBuffers(VulkanEngine::GetTransferCmp(), slot.primaryCmb);
				VulkanEngine::GetLogicalDevice().freeCommandBuffers(VulkanEngine::GetTransferCmp(), slot.secondaryCmb);
			}

			VulkanEngine::GetLogicalDevice().destroySemaphore(m_TimelineSemaphore);
		}

		VulkanStreamingLine::VulkanStreamingLine() {
			vk::CommandBufferAllocateInfo pCmbCreateInfo {
				.commandPool = VulkanEngine::GetTransferCmp(),
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = TRANSFERS_IN_FLIGHT
			};

			vk::CommandBufferAllocateInfo sCmbCreateInfo {
				.commandPool = VulkanEngine::GetTransferCmp(),
				.level = vk::CommandBufferLevel::eSecondary,
				.commandBufferCount = TRANSFERS_IN_FLIGHT
			};

			auto [result, pCmb] = VulkanEngine::GetLogicalDevice().allocateCommandBuffers(pCmbCreateInfo);
			auto [result1, sCmb] = VulkanEngine::GetLogicalDevice().allocateCommandBuffers(sCmbCreateInfo);

			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create primary command buffers for VulkanStreamingLine. Error: {}", vk::to_string(result));
			CORI_CORE_ASSERT(result1 == vk::Result::eSuccess, "Failed to create secondary command buffers for VulkanStreamingLine. Error: {}", vk::to_string(result1));

			for (uint32_t i = 0; i < TRANSFERS_IN_FLIGHT; i++) {
				auto& slot = m_Slots[i];

				slot.primaryCmb = pCmb[i];
				slot.secondaryCmb = sCmb[i];

				slot.pendingUploads.reserve(128);
			}

			vk::SemaphoreTypeCreateInfo semType {
				.semaphoreType = vk::SemaphoreType::eTimeline,
				.initialValue = 0
			};

			vk::SemaphoreCreateInfo semInfo {
				.pNext = &semType,
			};

			auto [result2, semaphore] = VulkanEngine::GetLogicalDevice().createSemaphore(semInfo);

			CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "Failed to create timeline semaphore for VulkanStreamingLine. Error: {}", vk::to_string(result2));

			m_TimelineSemaphore = semaphore;
			VulkanEngine::SetDebugName(m_TimelineSemaphore, "VulkanStreamingLine Timeline Semaphore");

			vk::BufferCreateInfo stagingCreateInfo {
				.size = HEAP_SIZE,
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive
			};

			vma::AllocationCreateInfo allocCreateInfo {
				.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
				.usage = vma::MemoryUsage::eAuto,
				.requiredFlags = vk::MemoryPropertyFlagBits::eHostCached
			};

			VulkanBuffer::CreateInfo stagingInfo {
				.bufferCreateInfo = &stagingCreateInfo,
				.allocationCreateInfo = &allocCreateInfo,
				.name = "VulkanStreamingLine ring staging buffer"
			};

			m_RingStagingBuffer = VulkanBuffer::Create(stagingInfo);

			//create PTEs ahead of time to not pay the price of some page faults during first couple of streams
			auto mapped = VulkanEngine::GetAllocator().mapMemory(m_RingStagingBuffer.m_Allocation);
			CORI_CORE_ASSERT(mapped.result == vk::Result::eSuccess, "Failed to map staging block for VulkanStreamingLine. Error: {}", vk::to_string(mapped.result));
			std::memset(mapped.value, 0, HEAP_SIZE);
			VulkanEngine::GetAllocator().unmapMemory(m_RingStagingBuffer.m_Allocation);

			vma::VirtualBlockCreateInfo blockInfo {
				.size = HEAP_SIZE,
				.flags = vma::VirtualBlockCreateFlagBits::eLinearAlgorithm
			};

			auto [result3, block] = vma::createVirtualBlock(blockInfo);
			CORI_CORE_ASSERT(result3 == vk::Result::eSuccess, "Failed to create vma virtual block for VulkanStreamingLine. Error: {}", vk::to_string(result3));

			m_StagingBlock = block;

			m_BufferBarriersCache.reserve(128);
			m_AcquireImageBarriersCache.reserve(128);
			m_ReleaseImageBarriersCache.reserve(128);
		}

		std::optional<uint32_t> VulkanStreamingLine::GetFreeTransferSlot() {
			uint32_t freeSlot = UINT32_MAX;
			uint32_t slotCounter = 0;
			auto [result, currentValue] = VulkanEngine::GetLogicalDevice().getSemaphoreCounterValue(Get().m_TimelineSemaphore);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to get value of the timeline semaphore in the streaming line. Error: {}", vk::to_string(result));

			for (const auto& slot : m_Slots) {
				if (slot.isBusy == false) {
					if (freeSlot == UINT32_MAX) {
						freeSlot = slotCounter;
					}
				} else if (currentValue >= slot.completionTicket) {
					ScrubSlot(slotCounter);

					if (freeSlot == UINT32_MAX) {
						freeSlot = slotCounter;
					}
				}

				slotCounter++;
			}

			if (freeSlot != UINT32_MAX) {
				return freeSlot;
			}

			return std::nullopt;
		}

		void VulkanStreamingLine::ScrubSlot(const uint32_t slotIndex) {
			auto& slot = m_Slots[slotIndex];
			slot.isBusy = false;

			for (auto& pending : slot.pendingUploads) {
				Get().m_StagingBlock.free(pending.stagingAllocation.virtualAllocation);
			}

			slot.pendingUploads.clear();
		}

		std::optional<VulkanStreamingLine::Allocation> VulkanStreamingLine::TryAllocate(const uint64_t size, const uint64_t alignment) {
			CORI_PROFILE_FUNCTION();
			if (size > HEAP_SIZE) {
				return std::nullopt;
			}

			vma::VirtualAllocationCreateInfo allocInfo {
				.size = size,
				.alignment = alignment,
				.flags = vma::VirtualAllocationCreateFlagBits::eStrategyMinTime
			};

			vk::DeviceSize offset;
			auto [result, virtAlloc] = m_StagingBlock.virtualAllocate(allocInfo, offset);
			if (result != vk::Result::eSuccess) {
				return std::nullopt;
			}

			Allocation allocation;
			allocation.offset = offset;
			allocation.virtualAllocation = virtAlloc;
			return allocation;
		}
	}
}