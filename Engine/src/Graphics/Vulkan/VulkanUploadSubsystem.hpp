#pragma once
#include <sul/dynamic_bitset.hpp>
#include "VulkanEngine.hpp"
#include "VulkanBuffer.hpp"
#include "DeletionQueue.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanVirtualBuffer {
		public:
			enum class Type {
				GPUScratch,
				CPUUpload
			};

			template<typename T = Byte>
			void UploadToAllocation(const std::span<T> data, uint64_t offset) {
				CORI_CORE_ASSERT(m_Type == Type::CPUUpload, "Calling UploadToAllocation on VulkanVirtualBuffer that was created as a GPU scratch, this type of virtual buffer can not be uploaded from CPU.")

				#ifdef DEBUG_BUILD
				if (alignof(T) > m_Alignment || m_Alignment % alignof(T) != 0) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Critical missalignment was encountered when trying to upload to VirtualBuffer, type '{}' has aligment '{}', but VirtualBuffer '{}' was created with alignment of '{}'. No upload was made.", CORI_CLEAN_TYPE_NAME(T), alignof(T), m_Name, m_Alignment);
					return;
				}
				#endif

				if (m_Alignment % offset != 0) {
					offset = Math::AlignUp(offset, m_Alignment);
					#ifdef DEBUG_BUILD
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Offset '{}' provided when calling UploadToAllocation of VirtualBuffer '{}' is missalignet to its alignment of '{}', it will be aligned up, this can cause errors.", offset, m_Name, m_Alignment);
					#else
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Offset '{}' provided when calling UploadToAllocation of VirtualBuffer '{}' is missalignet to its alignment of '{}', it will be aligned up, this can cause errors.", offset, "Name is unavailable in release build", m_Alignment);
					#endif
				}

				if (offset + data.size_bytes() > m_Size) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Trying to upload to VirtualBuffer '{}', but upload is out of bounds, offest '{}', data size '{}', buffer size '{}'", offset, data.size_bytes(), m_Size);
					return;
				}

				auto result = VulkanEngine::GetAllocator().copyMemoryToAllocation(data.data(), m_Alloc, offset + m_StartOffset, data.size_bytes());

				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy upload data to allocation of VulkanVirtualBuffer. Error: {}", vk::to_string(result));

				if (s_UploadListener) {
					s_UploadListener(*this);
				}
			}

			[[nodiscard]] vk::Buffer GetHeapHandle() {
				return m_Heap;
			}

			[[nodiscard]] uint64_t GetAlignment() const {
				return m_Alignment;
			}

			[[nodiscard]] uint64_t GetBDA() {
				return m_StartOffset;
			}

			[[nodiscard]] uint64_t GetStartOffset() const {
				return m_StartOffset;
			}

			[[nodiscard]] uint64_t GetSize() const {
				return m_Size;
			}

			[[nodiscard]] Type GetType() const {
				return m_Type;
			}

		protected:
			friend class VulkanVirtualBufferAllocator;
			VulkanVirtualBuffer() = default;

			Type m_Type;
			vk::Buffer m_Heap;
			vma::Allocation m_Alloc;
			uint64_t m_StartBDA{ 0 };
			uint64_t m_Size{ 0 };
			uint64_t m_StartOffset{ 0 };
			uint64_t m_Alignment{ 0 };
			#ifdef DEBUG_BUILD
			std::string m_Name{ "Unnamed VulkanVirtualBuffer" };
			#endif
			static inline std::function<void(const VulkanVirtualBuffer&)> s_UploadListener;
		};

		class VulkanVirtualBufferAllocator {
		public:
			~VulkanVirtualBufferAllocator() {
				DeletionQueue::PushBuffer(m_GPUScratchHeap, VulkanEngine::GetCurrentFrameInFlight());

				if (std::holds_alternative<VulkanBuffer>(m_UploadArenaHeap)) {
					DeletionQueue::PushBuffer(std::get<VulkanBuffer>(m_UploadArenaHeap), VulkanEngine::GetCurrentFrameInFlight());
				} else {
					auto [gpuArena, cpuArena] = std::get<std::pair<VulkanBuffer, VulkanBuffer>>(m_UploadArenaHeap);
					DeletionQueue::PushBuffer(gpuArena, VulkanEngine::GetCurrentFrameInFlight());
					DeletionQueue::PushBuffer(cpuArena, VulkanEngine::GetCurrentFrameInFlight());
				}

				DeletionQueue::PushVirtualBlock(m_GPUScratchBlock, VulkanEngine::GetCurrentFrameInFlight());
				DeletionQueue::PushVirtualBlock(m_UploadArenaBlock, VulkanEngine::GetCurrentFrameInFlight());
			}

			static VulkanVirtualBuffer CreateVirtualUploadBuffer(uint64_t size, const uint64_t alignment, const char* name = "") {
				size = Math::AlignUp(size, alignment);

				vma::VirtualAllocationCreateInfo allocInfo {
					.size = size,
					.alignment = alignment
				};

				vk::DeviceSize offset;
				auto [result, alloc] = Get().m_UploadArenaBlock.virtualAllocate(allocInfo, &offset);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create VulkanVirtualBuffer from CPU upload arena heap, likely out of memory. Error: {}", vk::to_string(result));

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
				DeletionQueue::PushVirtualAlloc(alloc, Get().m_UploadArenaBlock, VulkanEngine::GetCurrentFrameInFlight());
				return virtBuff;
			}

			static VulkanVirtualBuffer CreateVirtualScratchBuffer(uint64_t size, const uint64_t alignment, const char* name = "") {
				size = Math::AlignUp(size, alignment);

				vma::VirtualAllocationCreateInfo allocInfo {
					.size = size,
					.alignment = alignment
				};

				vk::DeviceSize offset;
				auto [result, alloc] = Get().m_GPUScratchBlock.virtualAllocate(allocInfo, &offset);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create VulkanVirtualBuffer from GPU scratch heap, likely out of memory. Error: {}", vk::to_string(result));

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
				DeletionQueue::PushVirtualAlloc(alloc, Get().m_GPUScratchBlock, VulkanEngine::GetCurrentFrameInFlight());
				return virtBuff;
			}

			static void SubmitCopies(vk::CommandBuffer cmb) {
				if (Get().m_UploadArenaType == UploadArenaType::STAGING) {
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
				}

			}

			static void Init();

			static void Shutdown();

			static VulkanVirtualBufferAllocator& Get();

		protected:
			friend VulkanVirtualBuffer;

			void ProcessUpload(const VulkanVirtualBuffer& buffer) {
				m_CopyRegions.emplace_back(vk::BufferCopy2{
					.srcOffset = buffer.m_StartOffset,
					.dstOffset = buffer.m_StartOffset,
					.size = buffer.m_Size
				});
			}

		private:
			VulkanVirtualBufferAllocator() {
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
			}

			enum class UploadArenaType {
				BAR,
				STAGING
			};

			struct BufferCopyRegion {
				uint64_t offset;
				uint64_t size;
			};

			std::variant<VulkanBuffer, std::pair<VulkanBuffer, VulkanBuffer>> m_UploadArenaHeap; // CPU visible heap
			std::vector<vk::BufferCopy2> m_CopyRegions; // Used only in case of STAGING mode fallback.
			UploadArenaType m_UploadArenaType;
			vma::VirtualBlock m_UploadArenaBlock;
			uint64_t m_UploadArenaBDA;

			VulkanBuffer m_GPUScratchHeap; // GPU only heap
			vma::VirtualBlock m_GPUScratchBlock;
			uint64_t m_GPUScratchBDA;

			static constexpr uint64_t UPLOAD_ARENA_SIZE{ 16 * 1024 * 1024 }; // 16mb
			static constexpr uint64_t GPU_SCRATCH_SIZE{ 128 * 1024 * 1024 }; // 128mb

			static constexpr vk::BufferUsageFlags HEAP_USAGE_FLAGS = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eVertexBuffer;

			static std::unique_ptr<VulkanVirtualBufferAllocator> s_Instance;
		};

		class VulkanDynamicContainerUploadManager {
			struct PendingCopy {
				vk::Buffer srcBuffer;
				vk::Buffer dstBuffer;
				uint64_t size;
			};
		public:
			~VulkanDynamicContainerUploadManager() {
				DeletionQueue::PushBuffer(m_RingStagingBuffer, VulkanEngine::GetCurrentFrameInFlight());

				DeletionQueue::PushVirtualBlock(m_RingStagingBlock, VulkanEngine::GetCurrentFrameInFlight());
			}

			static void Init();

			static void Shutdown();

			static VulkanDynamicContainerUploadManager& Get();

			static void ProcessUpdates(vk::CommandBuffer cmb) {
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

		protected:
			template <typename T> friend class VulkanDynamicVector;

			static void BeginUpdate(vk::Buffer dstBuffer) {
				Get().m_UploaderMutex.lock();

				auto& info = Get().m_BufferCopyInfos.emplace_back();

				info.dstBuffer = dstBuffer;
				info.srcBuffer = Get().m_RingStagingBuffer.m_Buffer;
				info.regionCount = 0;
				info.firstRegion = Get().m_CopyRegions.size();

			}

			static void Upload(const void* data, const uint64_t size, const uint64_t offset) {
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

				DeletionQueue::PushVirtualAlloc(virtAlloc, Get().m_RingStagingBlock, VulkanEngine::GetCurrentFrameInFlight());
			}

			static void EndUpdate() {
				Get().m_UploaderMutex.unlock();
			}



			static void RecordCopy(VulkanBuffer& srcBuffer, vk::Buffer dstBuffer, const uint64_t size) {
				CORI_CORE_ASSERT(Get().m_Initialized, "RecordCopy is called on non initialized VulkanDynamicContainerUploadManager.");
				Get().m_UploaderMutex.lock();

				Get().m_PendingCopies.emplace_back(srcBuffer.m_Buffer, dstBuffer, size);

				DeletionQueue::PushBuffer(srcBuffer, VulkanEngine::GetCurrentFrameInFlight());

				Get().m_UploaderMutex.unlock();
			}

			static void FallbackListener() {
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

		private:
			VulkanDynamicContainerUploadManager() = default;

			static constexpr uint64_t STAGING_SIZE{ 192 * 1024 * 1024 };

			VulkanBuffer m_RingStagingBuffer;
			vma::VirtualBlock m_RingStagingBlock;

			std::mutex m_UploaderMutex;

			std::array<std::vector<vma::VirtualAllocation>, FRAMES_IN_FLIGHT> m_DestructionQueue;
			std::vector<PendingCopy> m_PendingCopies;
			std::vector<vk::BufferCopy2> m_CopyRegions;

			struct CopyInfo {
				vk::Buffer srcBuffer;
				vk::Buffer dstBuffer;
				uint32_t regionCount;
				uint32_t firstRegion;
			};

			std::vector<CopyInfo> m_BufferCopyInfos;

			std::vector<vk::BufferMemoryBarrier2> m_BufferMemoryBarrierCache;

			static constexpr vk::PipelineStageFlags2 s_GenericReadStages =
				vk::PipelineStageFlagBits2::eVertexShader |
				vk::PipelineStageFlagBits2::eFragmentShader |
				vk::PipelineStageFlagBits2::eComputeShader;

			static constexpr vk::AccessFlags2 s_GenericReadAccess =
				vk::AccessFlagBits2::eShaderRead |
				vk::AccessFlagBits2::eUniformRead |
				vk::AccessFlagBits2::eShaderStorageRead;

			bool m_Initialized{ false };

			static std::unique_ptr<VulkanDynamicContainerUploadManager> s_Instance;
		};

		template <typename T>
		class VulkanDynamicVector {
			struct PendingReallocation {
				VulkanBuffer srcBuffer;
				uint64_t size{};
				bool active{ false };
			};
		public:


			VulkanDynamicVector(const QueueUsageFlags queueUsage, const vk::BufferUsageFlags bufferUsage, const char* name = "") : m_BufferUsageFlags(bufferUsage), m_QueueUsageFlags(queueUsage) {
				#ifdef DEBUG_BUILD
				if (!CORI_IS_EMPTY_CSTR(name)) {
					m_Name = std::string(name);
				}
				#endif
			}

			VulkanDynamicVector(const uint64_t capacity, const QueueUsageFlags queueUsage, const vk::BufferUsageFlags bufferUsage, const char* name = "") : m_BufferUsageFlags(bufferUsage), m_QueueUsageFlags(queueUsage) {
				Reserve(capacity);
				#ifdef DEBUG_BUILD
				if (!CORI_IS_EMPTY_CSTR(name)) {
					m_Name = std::string(name);
				}
				#endif
			}

			~VulkanDynamicVector() {
				if (m_GPUBuffers[0].m_Buffer) {
					for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
						DeletionQueue::PushBuffer(m_GPUBuffers[i], i);
					}
				}
			}

			VulkanDynamicVector(const VulkanDynamicVector&) = delete;
			VulkanDynamicVector& operator=(const VulkanDynamicVector&) = delete;

			VulkanDynamicVector(VulkanDynamicVector&& other)  noexcept {
				m_CPUShadow = std::move(other.m_CPUShadow);
				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_GPUBuffers[i] = other.m_GPUBuffers[i];
					other.m_GPUBuffers[i] = VulkanBuffer{};
					m_SectorStates[i] = std::move(m_SectorStates[i]);
					m_PendingReallocations = m_PendingReallocations[i];
					m_PendingReallocations[i] = PendingReallocation{};
					m_BufferUsageFlags = other.m_BufferUsageFlags;
					m_QueueUsageFlags = other.m_QueueUsageFlags;
					#ifdef DEBUG_BUILD
					m_Name = std::move(other.m_Name);
					other.m_Name = "Unnamed VulkanDynamicVector";
					#endif
					m_LastSectorSize = other.m_LastSectorSize;
					other.m_LastSectorSize = SECTOR_SIZE;
					m_IsBAR = other.m_IsBAR;
					other.m_IsBAR = false;
					m_IsDirty = other.m_IsDirty;
					other.m_IsDirty = false;
					m_ResizeRequired = other.m_ResizeRequired;
					other.m_ResizeRequired = false;
					m_OldSize = other.m_OldSize;
					other.m_OldSize = 0;
				}
			}

			VulkanDynamicVector& operator=(VulkanDynamicVector&& other)  noexcept {
				m_CPUShadow = std::move(other.m_CPUShadow);
				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_GPUBuffers[i] = other.m_GPUBuffers[i];
					other.m_GPUBuffers[i] = VulkanBuffer{};
					m_SectorStates[i] = std::move(m_SectorStates[i]);
					m_PendingReallocations = m_PendingReallocations[i];
					m_PendingReallocations[i] = PendingReallocation{};
					m_BufferUsageFlags = other.m_BufferUsageFlags;
					m_QueueUsageFlags = other.m_QueueUsageFlags;
					#ifdef DEBUG_BUILD
					m_Name = std::move(other.m_Name);
					other.m_Name = "Unnamed VulkanDynamicVector";
					#endif
					m_LastSectorSize = other.m_LastSectorSize;
					other.m_LastSectorSize = SECTOR_SIZE;
					m_IsBAR = other.m_IsBAR;
					other.m_IsBAR = false;
					m_IsDirty = other.m_IsDirty;
					other.m_IsDirty = false;
					m_ResizeRequired = other.m_ResizeRequired;
					other.m_ResizeRequired = false;
					m_OldSize = other.m_OldSize;
					other.m_OldSize = 0;
				}

				return *this;
			}

			struct ReferenceProxy {
				VulkanDynamicVector* parent;
				uint64_t index;

				ReferenceProxy& operator=(const T& value) {
					parent->m_CPUShadow[index] = value;
					parent->ReportChange(index * sizeof(T), sizeof(T));
					return *this;
				}

				ReferenceProxy& operator=(const ReferenceProxy& other) {
					if (parent == other.parent && index == other.index) {
						return *this;
					}

					parent->m_CPUShadow[index] = other.parent->m_CPUShadow[other.index];
					parent->ReportChange(index * sizeof(T), sizeof(T));
					return *this;
				}

				[[nodiscard]] T* operator->() {
					parent->ReportChange(index * sizeof(T), sizeof(T));
					return &parent->m_CPUShadow[index];
				}

				[[nodiscard]] const T* operator->() const {
					return &parent->m_CPUShadow[index];
				}

				[[nodiscard]] T& Get() {
					parent->ReportChange(index * sizeof(T), sizeof(T));
					return parent->m_CPUShadow[index];
				}

				operator const T&() const {
					return parent->m_CPUShadow[index];
				}

			};

			struct IteratorImpl {
				using iterator_category = std::random_access_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using value_type = T;
				using pointer = ReferenceProxy;
				using reference = ReferenceProxy;

				[[nodiscard]] ReferenceProxy operator*() const {
					return { m_Parent, static_cast<uint64_t>(m_Underlying - m_Parent->m_CPUShadow.begin()) };
				}

				[[nodiscard]] ReferenceProxy operator->() const {
					return operator*();
				}

				IteratorImpl& operator++() {
					++m_Underlying;
					return *this;
				}

				IteratorImpl operator++(int32_t) {
					IteratorImpl tmp = *this;
					++m_Underlying;
					return tmp;
				}

				IteratorImpl& operator--() {
					--m_Underlying;
					return *this;
				}

				IteratorImpl operator--(int32_t) {
					IteratorImpl tmp = *this;
					--m_Underlying;
					return tmp;
				}

				IteratorImpl& operator+=(difference_type n) {
					m_Underlying += n;
					return *this;
				}

				[[nodiscard]] IteratorImpl operator+(difference_type n) const {
					return { m_Parent, m_Underlying + n };
				}

				[[nodiscard]] difference_type operator-(const IteratorImpl& other) const {
					return m_Underlying - other.m_Underlying;
				}

				[[nodiscard]] bool operator!=(const IteratorImpl& other) const {
					return m_Underlying != other.m_Underlying;
				}

				[[nodiscard]] bool operator==(const IteratorImpl& other) const {
					return m_Underlying == other.m_Underlying;
				}

				VulkanDynamicVector* m_Parent;
			protected:
				friend VulkanDynamicVector;
				typename std::vector<T>::iterator m_Underlying;
			};

			using Iterator = IteratorImpl;
			using ConstIterator = std::vector<T>::const_iterator;

			using Reference = ReferenceProxy;
			using ConstReference = const T&;

			//TODO: range based variant
			void InsertRange(const std::span<T>& data, uint64_t offset) {
				if (offset + data.size() >= m_CPUShadow.capacity()) {
					uint64_t newCap = std::max(m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * 2, m_CPUShadow.capacity() + data.size());
					Reserve(newCap);
				}

				if (offset + data.size() >= m_CPUShadow.size()) {
					ResizeNonReporting(offset + data.size());
				}

				ReportChange(offset * sizeof(T), data.size_bytes());
				memcpy(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_CPUShadow.data()) + offset), data.data(), data.size_bytes());
			}

			//TODO: range based variant
			void AppendRange(const std::span<T>& data) {
				uint64_t changeStart = m_CPUShadow.size();
				if (changeStart + data.size() >= m_CPUShadow.capacity()) {
					uint64_t newCap = std::max(m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * 2, m_CPUShadow.capacity() + data.size());
					Reserve(newCap);
				}

				ResizeNonReporting(changeStart + data.size());

				ReportChange(changeStart * sizeof(T), data.size_bytes());
				memcpy(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_CPUShadow.data()) + changeStart), data.data(), data.size_bytes());
			}

			void PushBack(const T& value) {
				if (m_CPUShadow.size() == m_CPUShadow.capacity()) {
					size_t newCap = m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * GROWTH_FACTOR;
					Reserve(newCap);
				}

				uint64_t index = m_CPUShadow.size();
				m_CPUShadow.push_back(value);

				ReportChange(index * sizeof(T), sizeof(T));
			}

			void PushBack(T&& value) {
				EmplaceBack(std::move(value));
			}

			template <typename... Args>
			Reference EmplaceBack(Args&&... args) {
				if (m_CPUShadow.size() == m_CPUShadow.capacity()) {
					uint64_t newCap = m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * GROWTH_FACTOR;
					Reserve(newCap);
				}

				uint64_t index = m_CPUShadow.size();
				ReportChange(index * sizeof(T), sizeof(T));
				m_CPUShadow.emplace_back(std::forward<Args>(args)...);

				return (*this)[index];
			}

			void PopBack() {
				if (!m_CPUShadow.empty()) {
					m_CPUShadow.pop_back();
				}
			}

			[[nodiscard]] ConstReference operator[](const uint64_t index) const {
				#ifdef DEBUG_BUILD
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector '{}' index '{}' out of bounds.", m_Name, index);
				#else
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector '{}' index '{}' out of bounds.", "name unavailable in release build", index);
				#endif
				return m_CPUShadow[index];
			}

			[[nodiscard]] Reference operator[](const uint64_t index) {
				#ifdef DEBUG_BUILD
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector '{}' index '{}' out of bounds.", m_Name, index);
				#else
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector '{}' index '{}' out of bounds.", "name unavailable in release build", index);
				#endif
				return ReferenceProxy{ this, index };
			}

			[[nodiscard]] ConstReference At(const uint64_t index) const {
				return (*this)[index];
			}

			[[nodiscard]] Reference At(const uint64_t index) {
				return (*this)[index];
			}

			[[nodiscard]] ConstReference Back() const {
				#ifdef DEBUG_BUILD
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Back on empty VulkanDynamicVector '{}'.", m_Name);
				#else
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Back on empty VulkanDynamicVector '{}'.", "name unavailable in release build");
				#endif
				return m_CPUShadow.back();
			}

			[[nodiscard]] Reference Back() {
				#ifdef DEBUG_BUILD
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Back on empty VulkanDynamicVector '{}'.", m_Name);
				#else
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Back on empty VulkanDynamicVector '{}'.", "name unavailable in release build");
				#endif
				return (*this)[m_CPUShadow.size() - 1];
			}

			[[nodiscard]] ConstReference Front() const {
				#ifdef DEBUG_BUILD
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Front on empty VulkanDynamicVector '{}'.", m_Name);
				#else
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Front on empty VulkanDynamicVector '{}'.", "name unavailable in release build");
				#endif
				return m_CPUShadow.front();
			}

			[[nodiscard]] Reference Front() {
				#ifdef DEBUG_BUILD
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Front on empty VulkanDynamicVector '{}'.", m_Name);
				#else
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Front on empty VulkanDynamicVector '{}'.", "name unavailable in release build");
				#endif
				return (*this)[0];
			}

			Iterator begin() {
				return { this, m_CPUShadow.begin() };
			}

			Iterator end() {
				return { this, m_CPUShadow.end() };
			}

			Iterator rbegin() {
				return { this, m_CPUShadow.rbegin() };
			}

			Iterator rend() {
				return { this, m_CPUShadow.rend() };
			}

			ConstIterator begin() const {
				return m_CPUShadow.begin();
			}

			ConstIterator end() const {
				return m_CPUShadow.end();
			}

			ConstIterator rbegin() const {
				return m_CPUShadow.rbegin();
			}

			ConstIterator rend() const {
				return m_CPUShadow.rend();
			}

			ConstIterator cbegin() const {
				return m_CPUShadow.cbegin();
			}

			ConstIterator cend() const {
				return m_CPUShadow.cend();
			}

			ConstIterator crbegin() const {
				return m_CPUShadow.crbegin();
			}

			ConstIterator crend() const {
				return m_CPUShadow.crend();
			}

			void Clear() {
				m_CPUShadow.clear();
			}

			[[nodiscard]] uint64_t Size() const {
				return m_CPUShadow.size();
			}

			[[nodiscard]] uint64_t Capacity() const {
				return m_CPUShadow.capacity();
			}

			[[nodiscard]] bool Empty() const {
				return m_CPUShadow.empty();
			}

			void Sync() {
				if (m_ResizeRequired) {
					ResizeBuffers(m_CPUShadow.capacity());
					m_ResizeRequired = false;
				}

				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();

				auto& reaclloc = m_PendingReallocations[frameIndex];
				if (reaclloc.active) {
					reaclloc.active = false;

					if (m_IsBAR) {
						ReportChange(0, reaclloc.size);
						reaclloc.srcBuffer.Destroy();
					}
					else {
						VulkanDynamicContainerUploadManager::RecordCopy(reaclloc.srcBuffer, m_GPUBuffers[frameIndex].m_Buffer, reaclloc.size);
					}
				}

				if (m_IsDirty) {

					if (!m_IsBAR) {
						VulkanDynamicContainerUploadManager::BeginUpdate(m_GPUBuffers[frameIndex].m_Buffer);
					}

					uint64_t currentRangeBeginning = m_SectorStates[frameIndex].find_first();
					while (currentRangeBeginning != sul::dynamic_bitset<>::npos) {
						uint64_t currentRangeEnd = m_SectorStates[frameIndex].find_sequence_end(currentRangeBeginning);

						uint64_t offset = currentRangeBeginning * SECTOR_SIZE;
						uint64_t size = (currentRangeEnd - currentRangeBeginning) * SECTOR_SIZE;

						size += m_LastSectorSize;

						if (m_IsBAR) {
							auto result = VulkanEngine::GetAllocator().copyMemoryToAllocation(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_CPUShadow.data()) + offset), m_GPUBuffers[frameIndex].m_Allocation, offset, size);
							CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy upload data to allocation of VulkanDynamicVector. Error: {}", vk::to_string(result));
						} else {
							VulkanDynamicContainerUploadManager::Upload(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_CPUShadow.data()) + offset), size, offset);
						}

						currentRangeBeginning = m_SectorStates[frameIndex].find_next(currentRangeEnd);
					}

					if (!m_IsBAR) {
						VulkanDynamicContainerUploadManager::EndUpdate();
					}

					m_SectorStates[frameIndex].reset();

					m_IsDirty = false;
				}
			}

			void Reserve(const uint64_t newCapacity) {
				if (newCapacity <= m_CPUShadow.capacity()) {
					return;
				}

				m_ResizeRequired = true;
				m_OldSize = m_CPUShadow.size();
				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_SectorStates[i].resize(std::ceil(static_cast<float>(newCapacity * sizeof(T)) / static_cast<float>(SECTOR_SIZE)), false);
				}

				m_CPUShadow.reserve(newCapacity);
			}

			void Resize(const uint64_t newSize, const T& value) {
				if (newSize <= m_CPUShadow.size()) {
					return;
				}

				if (newSize > m_CPUShadow.capacity()) {
					Reserve(newSize);
				}

				ReportChange(m_CPUShadow.size() * sizeof(T), (newSize - m_CPUShadow.size()) * sizeof(T));
				m_CPUShadow.resize(newSize, value);
			}

			void Resize(const uint64_t newSize) {
				if (newSize <= m_CPUShadow.size()) {
					return;
				}

				if (newSize > m_CPUShadow.capacity()) {
					Reserve(newSize);
				}

				ReportChange(m_CPUShadow.size() * sizeof(T), (newSize - m_CPUShadow.size()) * sizeof(T));
				m_CPUShadow.resize(newSize);
			}

			[[nodiscard]] VulkanBuffer& GetVulkanBuffer() {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				return m_GPUBuffers[frameIndex];
			}

		private:
			void ReportChange(const uint64_t startOffset, const uint64_t size) {
				uint32_t affectedSectorStart = std::floor(startOffset / SECTOR_SIZE);
				uint32_t affectedSectorEnd = std::floor((startOffset + size - 1) / SECTOR_SIZE);

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_SectorStates[i].set(affectedSectorStart, affectedSectorEnd - affectedSectorStart + 1, true);
				}

				m_IsDirty = true;
			}

			void ResizeNonReporting(const uint64_t newSize) {
				if (newSize <= m_CPUShadow.size()) {
					return;
				}

				if (newSize > m_CPUShadow.capacity()) {
					Reserve(newSize);
				}

				m_CPUShadow.resize(newSize);
			}

			void ResizeBuffers(const uint64_t newSize) {
				auto& sharingSettings = VulkanEngine::GetBufferSharingSettings(m_QueueUsageFlags);
				vk::BufferCreateInfo bufferCreateInfo{
					.size = newSize * sizeof(T),
					.usage = m_BufferUsageFlags | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
					.sharingMode = sharingSettings.first,
					.queueFamilyIndexCount = static_cast<uint32_t>(sharingSettings.second.size()),
					.pQueueFamilyIndices = sharingSettings.second.data()
				};

				vma::AllocationCreateInfo BARAlloc {
					.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead | vma::AllocationCreateFlagBits::eMapped,
					.usage = vma::MemoryUsage::eAuto,
					.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
				};

				vma::AllocationCreateInfo localAlloc {
					.usage = vma::MemoryUsage::eAuto,
					.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
				};


				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
					VulkanBuffer::CreateInfo info {
						.bufferCreateInfo = &bufferCreateInfo,
						.allocationCreateInfo = &BARAlloc,
					};

					if (m_GPUBuffers[i].m_Buffer) {
						m_PendingReallocations[i].srcBuffer = m_GPUBuffers[i];
						m_PendingReallocations[i].size = sizeof(T) * m_OldSize;
						m_PendingReallocations[i].active = true;
					}

					#ifdef DEBUG_BUILD
					std::string name = std::format("{} GPU buffer {}", m_Name, i);
					info.name = name.c_str();
					#endif

					m_GPUBuffers[i] = VulkanBuffer::Create(info);
				}

				std::array<bool, FRAMES_IN_FLIGHT> isBufferBAR{ true };
				m_IsBAR = true;

				uint32_t lastSectorSize = newSize % SECTOR_SIZE;
				m_LastSectorSize = lastSectorSize != 0 ? lastSectorSize : SECTOR_SIZE;

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					auto propertyFlags = VulkanEngine::GetAllocator().getAllocationMemoryProperties(m_GPUBuffers[i].m_Allocation);
					if (!(propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) || true) {
						isBufferBAR[i] = false;
						m_IsBAR = false;
					}
				}

				if (!m_IsBAR) {
					VulkanDynamicContainerUploadManager::FallbackListener();
					for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
						if (isBufferBAR[i]) {

							VulkanBuffer::CreateInfo info {
								.bufferCreateInfo = &bufferCreateInfo,
								.allocationCreateInfo = &localAlloc,
							};

							m_GPUBuffers[i].Destroy();

							#ifdef DEBUG_BUILD
							std::string name = std::format("{} GPU buffer {}", m_Name, i);
							info.name = name.c_str();
							#endif

							m_GPUBuffers[i] = VulkanBuffer::Create(info);
						}
					}
				}
			}

			std::vector<T> m_CPUShadow;
			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_GPUBuffers;
			std::array<sul::dynamic_bitset<>, FRAMES_IN_FLIGHT> m_SectorStates;
			std::array<PendingReallocation, FRAMES_IN_FLIGHT> m_PendingReallocations;
			vk::BufferUsageFlags m_BufferUsageFlags;
			QueueUsageFlags m_QueueUsageFlags;
			#ifdef DEBUG_BUILD
			std::string m_Name{ "Unnamed VulkanDynamicVector" };
			#endif
			uint32_t m_LastSectorSize{ SECTOR_SIZE };
			bool m_IsBAR{ false };
			bool m_IsDirty{ false };
			bool m_ResizeRequired{ false };
			uint64_t m_OldSize{ 0 };

			static constexpr uint32_t SECTOR_SIZE{ 4 * 1024 }; //4kb
			static constexpr float GROWTH_FACTOR{ 2.0f };

		};

		template<typename T>
		struct VulkanFlatSlotMapHandle : Core::VersionedHandleBase {};
		
		template<typename T, Core::IsVersionedHandle HandleT = VulkanFlatSlotMapHandle<T>>
		class VulkanFlatSlotMap {
		public:
			using Handle = HandleT;

			template <bool IsConst>
			struct IteratorImpl {
				using iterator_category = std::forward_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using value_type = T;
				using pointer = typename std::conditional<IsConst, const T*, typename VulkanDynamicVector<T>::Reference>::type;
				using reference = typename std::conditional<IsConst, typename VulkanDynamicVector<T>::ConstReference, typename VulkanDynamicVector<T>::Reference>::type;
				using MapType = typename std::conditional<IsConst, const VulkanFlatSlotMap, VulkanFlatSlotMap>::type;

				void SkipForward() {
					while (m_Index != m_Map->Size() && m_Map->IsIndexValid(m_Index + 1)) {
						m_Index++;
					}
				}

				void SkipBackwards() {
					while (m_Index > 0 && m_Map->IsIndexValid(m_Index - 1)) {
						m_Index--;
					}
				}

				IteratorImpl& operator++() {
					SkipForward();
					return *this;
				}

				IteratorImpl operator++(int32_t) {
					IteratorImpl tmp = *this;
					++(*this);
					return tmp;
				}

				IteratorImpl& operator--() {
					SkipBackwards();
					return *this;
				}

				IteratorImpl operator--(int32_t) {
					IteratorImpl tmp = *this;
					--(*this);
					return tmp;
				}

				[[nodiscard]] bool operator==(const IteratorImpl& other) const {
					return m_Index == other.m_Index && m_Map == other.m_Map;
				}

				[[nodiscard]] bool operator!=(const IteratorImpl& other) const {
					return !(*this == other);
				}

				[[nodiscard]] reference operator*() const {
					return (*m_Map)[m_Index];
				}

				[[nodiscard]] reference operator->() const {
					if constexpr (IsConst) {
						return &(*m_Map)[m_Index];
					}
					else {
						return (*m_Map)[m_Index];
					}
				}

				MapType* m_Map;

			protected:
				friend VulkanFlatSlotMap;
				uint32_t m_Index;
				IteratorImpl(MapType* m, const uint32_t index) : m_Map(m), m_Index(index) {}

			};

			using Iterator = IteratorImpl<false>;
			using ConstIterator = IteratorImpl<true>;
			using Reference = VulkanDynamicVector<T>::Reference;
			using ConstReference = VulkanDynamicVector<T>::ConstReference;
			using ReverseIterator = std::reverse_iterator<Iterator>;
			using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

			VulkanFlatSlotMap(const QueueUsageFlags queueUsage, const vk::BufferUsageFlags bufferUsage, const char* name = "") : m_Data(queueUsage, bufferUsage, name) {
				m_Holes.reserve(INITIAL_HOLE_VECTOR_SIZE);
			}

			VulkanFlatSlotMap(const uint64_t capacity, const QueueUsageFlags queueUsage, const vk::BufferUsageFlags bufferUsage, const char* name = "") : m_Data(queueUsage, bufferUsage, name) {
				m_Versions.reserve(capacity);
				m_Holes.reserve(INITIAL_HOLE_VECTOR_SIZE);
			}

			VulkanFlatSlotMap(const VulkanFlatSlotMap&) = delete;
			VulkanFlatSlotMap& operator=(const VulkanFlatSlotMap&) = delete;

			VulkanFlatSlotMap(VulkanFlatSlotMap&& other) = default;
			VulkanFlatSlotMap& operator=(VulkanFlatSlotMap&& other) = default;

			void Reserve(uint32_t capacity) {
				m_Data.Reserve(capacity);
				m_Versions.reserve(capacity);
			}

			template<typename... Args>
			[[nodiscard]] Handle Insert(Args&&... args) {
				uint32_t index;
				if (!m_Holes.empty()) {
					index = m_Holes.back();
					m_Holes.pop_back();

					m_Data[index] = T(std::forward<Args>(args)...);

					m_Versions[index] = -m_Versions[index] + 1;
				} else {
					index = m_Data.size();
					m_Data.EmplaceBack(std::forward<Args>(args)...);
					m_Versions.emplace_back(1);
				}

				return { index, m_Versions[index] };
			}

			void Remove(const Handle handle) {
				if (!IsHandleValid(handle)) {
					return;
				}

				m_Data[handle.GetIndex()] = T{};

				m_Versions[handle.GetIndex()] *= -1;
				m_Holes.emplace_back(handle.GetIndex());
			}

			[[nodiscard]] std::optional<Reference> TryGet(const Handle handle) {
				if (IsHandleValid(handle)) {
					return m_Data[handle.GetIndex()];
				}

				return std::nullopt;
			}

			[[nodiscard]] std::optional<std::reference_wrapper<const T>> TryGet(const Handle handle) const {
				if (IsHandleValid(handle)) {
					return std::cref(m_Data[handle.GetIndex()]);
				}

				return std::nullopt;
			}

			[[nodiscard]] Reference operator[](const Handle handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[handle.GetIndex()];
			}

			[[nodiscard]] ConstReference operator[](const Handle handle) const {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[handle.GetIndex()];
			}

			[[nodiscard]] Iterator begin() {
				Iterator it(this, 0);
				if (!IsIndexValid(0)) {
					it.SkipForward();
				}

				return it;
			}

			[[nodiscard]] Iterator end() {
				Iterator it(this, static_cast<uint32_t>(m_Data.RawSize()));
				if (!IsIndexValid(static_cast<uint32_t>(m_Data.RawSize()))) {
					it.SkipBackward();
				}

				return it;
			}

			[[nodiscard]] ReverseIterator rbegin() {
				return ReverseIterator(end());
			}

			[[nodiscard]] ReverseIterator rend() {
				return ReverseIterator(begin());
			}

			[[nodiscard]] ConstIterator begin() const {
				ConstIterator it(this, 0);
				if (!IsIndexValid(0)) {
					it.SkipForward();
				}

				return it;
			}

			[[nodiscard]] ConstIterator end() const {
				ConstIterator it(this, static_cast<uint32_t>(m_Data.RawSize()));
				return it;
			}

			[[nodiscard]] ConstIterator cbegin() const {
				return begin();
			}

			[[nodiscard]] ConstIterator cend() const {
				return end();
			}

			[[nodiscard]] ConstReverseIterator rbegin() const {
				return ConstReverseIterator(end());
			}

			[[nodiscard]] ConstReverseIterator rend() const {
				return ConstReverseIterator(begin());
			}

			[[nodiscard]] ConstReverseIterator crbegin() const {
				return ConstReverseIterator(end());
			}

			[[nodiscard]] ConstReverseIterator crend() const {
				return ConstReverseIterator(begin());
			}

			[[nodiscard]] uint64_t Size() const {
				return m_Data.Size() - m_Holes.size();
			}

			[[nodiscard]] uint64_t RawSize() const {
				return m_Data.Size();
			}

			[[nodiscard]] uint64_t Capacity() const {
				return m_Data.Capacity();
			}

			[[nodiscard]] bool Empty() const {
				return Size() == 0;
			}

			[[nodiscard]] bool IsHandleValid(const Handle handle) {
				return handle.GetIndex() < RawSize() && m_Versions[handle.GetIndex()] == handle.GetVersion();
			}

			[[nodiscard]] bool IsIndexValid(const uint32_t index) const {
				return index < RawSize() && 0 < m_Versions[index];
			}

			void Clear() {
				m_Data.clear();
				m_Versions.clear();
				m_Holes.clear();
			}

		protected:
			friend Iterator;
			friend ConstIterator;

			[[nodiscard]] Reference operator[](const uint32_t index) {
				CORI_CORE_ASSERT(index < m_Data.Size(), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[index];
			}

			[[nodiscard]] ConstReference operator[](const uint32_t index) const {
				CORI_CORE_ASSERT(index < m_Data.Size(), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[index];
			}

			VulkanDynamicVector<T> m_Data{};
		private:
			std::vector<int32_t> m_Versions{};
			std::vector<uint32_t> m_Holes{};
			static constexpr uint32_t INITIAL_HOLE_VECTOR_SIZE{ 32 };
		};
	}
}