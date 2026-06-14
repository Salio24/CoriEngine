#pragma once
#include <sul/dynamic_bitset.hpp>
#include <utility>
#include "VulkanEngine.hpp"
#include "VulkanBuffer.hpp"
#include "DeletionQueue.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "Utility/TemplateUtils.hpp"
#include "Utility/BitHelpers.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanVirtualBuffer {
		public:
			enum class Type {
				GPUScratch,
				CPUUpload
			};

			template<typename T = Byte>
			bool UploadToAllocation(const std::span<T> data, uint64_t offset) {
				CORI_PROFILE_FUNCTION();

				CORI_CORE_ASSERT(m_Type == Type::CPUUpload, "Calling UploadToAllocation on VulkanVirtualBuffer that was created as a GPU scratch, this type of virtual buffer can not be uploaded from CPU.")

				#ifdef DEBUG_BUILD
				if (alignof(T) > m_Alignment || m_Alignment % alignof(T) != 0) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Critical misalignment was encountered when trying to upload to VirtualBuffer, type '{}' has alignment '{}', but VirtualBuffer '{}' was created with alignment of '{}'. No upload was made.", CORI_CLEAN_TYPE_NAME(T), alignof(T), m_Name, m_Alignment);
					return false;
				}
				#endif

				if (offset % m_Alignment != 0) {
					offset = Math::AlignUp(offset, m_Alignment);
					#ifdef DEBUG_BUILD
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Offset '{}' provided when calling UploadToAllocation of VirtualBuffer '{}' is misaligned to its alignment of '{}', no upload will be made.", offset, m_Name, m_Alignment);
					#else
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Offset '{}' provided when calling UploadToAllocation of VirtualBuffer '{}' is misaligned to its alignment of '{}', no upload will be made.", offset, "Name is unavailable in release build", m_Alignment);
					#endif
					return false;
				}

				if (offset + data.size_bytes() > m_Size) {
					#ifdef DEBUG_BUILD
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Trying to upload to VirtualBuffer '{}', but upload is out of bounds, offest '{}', data size '{}', buffer size '{}', no upload will be made.", m_Name, offset, data.size_bytes(), m_Size);
					#else
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Trying to upload to VirtualBuffer '{}', but upload is out of bounds, offest '{}', data size '{}', buffer size '{}', no upload will be made.", "Name is unavailable in release build", offset, data.size_bytes(), m_Size);
					#endif
					return false;
				}

				auto result = VulkanEngine::GetAllocator().copyMemoryToAllocation(data.data(), m_Alloc, offset + m_StartOffset, data.size_bytes());

				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy upload data to allocation of VulkanVirtualBuffer. Error: {}", vk::to_string(result));

				if (s_UploadListener) {
					s_UploadListener(*this);
				}

				return true;
			}

			[[nodiscard]] vk::Buffer GetHeapHandle() {
				return m_Heap;
			}

			[[nodiscard]] uint64_t GetAlignment() const {
				return m_Alignment;
			}

			[[nodiscard]] uint64_t GetBDA() {
				return m_StartBDA;
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
			friend class RenderGraph;
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

		//TODO: inject poison value to uninitialized memory in debug build in heaps

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

			static VulkanVirtualBuffer CreateVirtualUploadBuffer(uint64_t size, const uint64_t alignment, const uint32_t dstFrameInFlight, const char* name = "") {
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

			static VulkanVirtualBuffer CreateVirtualScratchBuffer(uint64_t size, const uint64_t alignment, const uint32_t dstFrameInFlight,  const char* name = "") {
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

			static void ClearGPUScratchBlock(uint32_t frameInFlight) {
				for (auto& alloc : Get().m_GPUScratchVirtualAllocations[frameInFlight]) {
					Get().m_GPUScratchBlock.free(alloc);
				}

				Get().m_GPUScratchVirtualAllocations[frameInFlight].clear();

				for (auto& alloc : Get().m_UploadVirtualAllocations[frameInFlight]) {
					Get().m_UploadArenaBlock.free(alloc);
				}

				Get().m_UploadVirtualAllocations[frameInFlight].clear();
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

				for (auto& vec : m_GPUScratchVirtualAllocations) {
					vec.reserve(16);
				}

				for (auto& vec : m_UploadVirtualAllocations) {
					vec.reserve(16);
				}
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
			std::array<std::vector<vma::VirtualAllocation>, FRAMES_IN_FLIGHT> m_GPUScratchVirtualAllocations;
			std::array<std::vector<vma::VirtualAllocation>, FRAMES_IN_FLIGHT> m_UploadVirtualAllocations;


			static constexpr uint64_t UPLOAD_ARENA_SIZE{ 16 * 1024 * 1024 }; // 16mb
			static constexpr uint64_t GPU_SCRATCH_SIZE{ 128 * 1024 * 1024 }; // 128mb

			static constexpr vk::BufferUsageFlags HEAP_USAGE_FLAGS = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer;

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
				if (m_RingStagingBuffer.m_Buffer) {
					DeletionQueue::PushBuffer(m_RingStagingBuffer, VulkanEngine::GetCurrentFrameInFlight());
				}

				if (m_RingStagingBlock) {
					DeletionQueue::PushVirtualBlock(m_RingStagingBlock, VulkanEngine::GetCurrentFrameInFlight());
				}
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
			template <Utility::NotBool T> friend class VulkanDynamicVector;

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

		template <Utility::NotBool T>
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

			VulkanDynamicVector(VulkanDynamicVector&& other) noexcept {
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
					m_IsDirtyMask = other.m_IsDirtyMask;
					other.m_IsDirtyMask = false;
					m_ResizeRequired = other.m_ResizeRequired;
					other.m_ResizeRequired = false;
					m_OldSize = other.m_OldSize;
					other.m_OldSize = 0;
				}
			}

			VulkanDynamicVector& operator=(VulkanDynamicVector&& other) noexcept {
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
					m_IsDirtyMask = other.m_IsDirtyMask;
					other.m_IsDirtyMask = false;
					m_ResizeRequired = other.m_ResizeRequired;
					other.m_ResizeRequired = false;
					m_OldSize = other.m_OldSize;
					other.m_OldSize = 0;
				}

				return *this;
			}

			//FIXME: make it a proper std::contiguous_iterator
			struct IteratorImpl {
				using iterator_category = std::vector<T>::iterator::iterator_category;
				using difference_type = std::vector<T>::iterator::difference_type;
				using value_type = std::vector<T>::iterator::value_type;
				using pointer = std::vector<T>::iterator::pointer;
				using reference = std::vector<T>::iterator::reference;

				[[nodiscard]] reference operator*() const {
					auto index = static_cast<uint64_t>(m_Underlying - m_Parent->m_CPUShadow.begin());
					m_Parent->ReportChange(index * sizeof(T), sizeof(T));
					return *m_Underlying;
				}

				[[nodiscard]] pointer operator->() const {
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

				IteratorImpl& operator-=(difference_type n) {
					m_Underlying -= n;
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
			using ConstIterator = typename std::vector<T>::const_iterator;

			using Reference = T&;
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
				ReportChange(index * sizeof(T), sizeof(T));
				return m_CPUShadow[index];
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

				if (Utility::IsSet(m_IsDirtyMask, frameIndex)) {

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

					Utility::Reset(m_IsDirtyMask, frameIndex);
				}
			}

			//TODO: poison uninitialized memory in debug builds
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

				// using m_OldSize determine where the initialized data ends, and poison with something like 0xFC the range between after last initialized item to the allocation end
				// report the change of that uninitialized (now poisoned) range
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

			[[nodiscard]] const VulkanBuffer& GetVulkanBuffer() const  {
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

				m_IsDirtyMask = UINT8_MAX;
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
					if (!(propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)) {
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
			uint8_t m_IsDirtyMask{ 0 };
			bool m_ResizeRequired{ false };
			uint64_t m_OldSize{ 0 };

			static constexpr uint32_t SECTOR_SIZE{ 4 * 1024 }; //4kb
			static constexpr float GROWTH_FACTOR{ 2.0f };

		};

		template<std::copy_constructible T, uint16_t REUSE_THRESHOLD = 64, bool ENABLE_VERSIONING = true, Core::IsVersionedHandle HandleT = Core::Handle<T>, typename ConstHandleT = Core::ConstHandle<T>> requires std::derived_from<HandleT, ConstHandleT>
		class VulkanFlatSlotMap {
		public:
			using Handle = HandleT;
			using ConstHandle = ConstHandleT;

			using SizeT = uint32_t;

			template<bool IsConst>
			struct IteratorImpl {
				using iterator_category = std::bidirectional_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using value_type = T;
				using pointer = typename std::conditional<IsConst, const T*, T*>::type;
				using reference = typename std::conditional<IsConst, typename VulkanDynamicVector<T>::ConstReference, typename VulkanDynamicVector<T>::Reference>::type;
				using MapType = typename std::conditional<IsConst, const VulkanFlatSlotMap, VulkanFlatSlotMap>::type;

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

				template<bool OtherIsConst>
				[[nodiscard]] bool operator==(const IteratorImpl<OtherIsConst>& other) const {
					return m_Index == other.m_Index && m_Map == other.m_Map;
				}
				template<bool OtherIsConst>
				[[nodiscard]] bool operator!=(const IteratorImpl<OtherIsConst>& other) const {
					return !(*this == other);
				}

				[[nodiscard]] reference operator*() const {
					return (*m_Map)[m_Index];
				}

				[[nodiscard]] pointer operator->() const {
					return &(*m_Map)[m_Index];
				}

				[[nodiscard]] Handle GetHandle() const {
					return m_Map->GetIndexHandle(m_Index);
				}

				MapType* m_Map;

			protected:
				friend VulkanFlatSlotMap;
				SizeT m_Index;
				IteratorImpl(MapType* m, const SizeT index) : m_Map(m), m_Index(index) {}

			private:
				void SkipForward() {
					sul::dynamic_bitset<>::size_type nextIndex = m_Map->m_SlotStates.find_next(m_Index);

					if (nextIndex == sul::dynamic_bitset<>::npos) {
						m_Index = static_cast<uint32_t>(m_Map->m_Data.Capacity());
						return;
					}

					m_Index = nextIndex;
				}

				void SkipBackwards() {
					sul::dynamic_bitset<>::size_type prevIndex = m_Map->m_SlotStates.find_prev(m_Index);

					if (prevIndex != sul::dynamic_bitset<>::npos) {
						m_Index = prevIndex;
					}
				}
			};

			using Iterator = IteratorImpl<false>;
			using ConstIterator = IteratorImpl<true>;
			using Reference = typename VulkanDynamicVector<T>::Reference;
			using ConstReference = typename VulkanDynamicVector<T>::ConstReference;
			using ReverseIterator = std::reverse_iterator<Iterator>;
			using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

			VulkanFlatSlotMap(const QueueUsageFlags queueUsage, const vk::BufferUsageFlags bufferUsage, const char* name = "") : m_Data(queueUsage, bufferUsage, name) {}

			VulkanFlatSlotMap(const SizeT capacity, const QueueUsageFlags queueUsage, const vk::BufferUsageFlags bufferUsage, const char* name = "") : m_Data(queueUsage, bufferUsage, name) {
				Reserve(capacity);
			}

			VulkanFlatSlotMap(const VulkanFlatSlotMap&) = delete;
			VulkanFlatSlotMap& operator=(const VulkanFlatSlotMap&) = delete;

			VulkanFlatSlotMap(VulkanFlatSlotMap&& other) = default;
			VulkanFlatSlotMap& operator=(VulkanFlatSlotMap&& other) = default;

			void Reserve(SizeT capacity) {
				m_Data.Reserve(capacity);
				if constexpr (ENABLE_VERSIONING) {
					m_Versions.reserve(capacity);
				}

				m_SlotStates.reserve(capacity);
			}

			template<typename... Args>
			[[nodiscard]] Handle Emplace(Args&&... args) {
				SizeT index;

				const bool thresholdMet = m_Holes.size() > REUSE_THRESHOLD;
				const bool clearInProgress = m_ReusedIndexCounter > 0;

				if (thresholdMet || clearInProgress) {
					if (thresholdMet && !clearInProgress) {
						m_ReusedIndexCounter = m_Holes.size();
					}
					else {
						m_ReusedIndexCounter--;
					}

					index = m_Holes.front();
					m_Holes.pop_front();

					m_Data[index] = std::move(T(std::forward<Args>(args)...));

					if constexpr (ENABLE_VERSIONING) {
						m_Versions[index]++;
					}

					m_SlotStates[index] = true;
				} else {
					index = m_Data.Size();
					m_Data.EmplaceBack(std::forward<Args>(args)...);
					m_SlotStates.push_back(true);
					if constexpr (ENABLE_VERSIONING) {
						m_Versions.emplace_back(1);
					}
				}

				if constexpr (requires { m_Data[index].version = uint32_t{}; } && ENABLE_VERSIONING){
					m_Data[index].version = m_Versions[index];
				}

				if constexpr (requires { m_Data[index].valid = bool{}; }) {
					m_Data[index].valid = true;
				}

				if constexpr (ENABLE_VERSIONING) {
					return Handle{ index, m_Versions[index] };
				}

				return Handle{ index, 1 };
			}

			template<typename... Args>
			bool EmplaceAt(const SizeT index, Args&&... args) {
				static_assert(ENABLE_VERSIONING == false, "Index version of EmplaceAt should only be used with versioning off.");

				if (index >= RawSize()) {
					return false;
				}

				if (IsIndexOccupied(index)) {
					return false;
				}

				if constexpr (requires { m_Data[index].valid = bool{}; }) {
					m_Data[index].valid = true;
				}

				m_Data[index] = std::move(T(std::forward<Args>(args)...));

				m_SlotStates[index] = true;

				return true;
			}

			void Remove(const Handle handle) {
				if (!IsHandleValid(handle)) {
					return;
				}

				SizeT index = handle.GetIndex();

				m_Data[index] = T{};

				m_SlotStates[index] = false;
				m_Holes.emplace_back(index);
			}

			void RemoveAt(const SizeT index) {
				static_assert(ENABLE_VERSIONING == false, "Index version of RemoveAt should only be used with versioning off.");

				if (!IsIndexValid(index)) {
					return;
				}

				m_Data[index] = T{};

				m_SlotStates[index] = false;
			}

			[[nodiscard]] std::optional<Reference> TryGet(const Handle handle) {
				if (IsHandleValid(handle)) {
					return m_Data[handle.GetIndex()];
				}

				return std::nullopt;
			}

			[[nodiscard]] std::optional<std::reference_wrapper<const T>> TryGet(const ConstHandle handle) const {
				if (IsHandleValid(handle)) {
					return std::cref(std::as_const(m_Data)[handle.GetIndex()]);
				}

				return std::nullopt;
			}

			[[nodiscard]] Reference operator[](const Handle handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[handle.GetIndex()];
			}

			[[nodiscard]] ConstReference operator[](const ConstHandle handle) const {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed FlatSlotMap with an invalid handle.");
				return std::as_const(m_Data)[handle.GetIndex()];
			}

			[[nodiscard]] Reference operator[](const SizeT index) {
				CORI_CORE_ASSERT(index < m_Data.Size() && m_SlotStates[index], "Accessed FlatSlotMap with an invalid index.");
				return m_Data[index];
			}

			[[nodiscard]] ConstReference operator[](const SizeT index) const {
				CORI_CORE_ASSERT(index < m_Data.Size() && m_SlotStates[index], "Accessed FlatSlotMap with an invalid index.");
				return std::as_const(m_Data)[index];
			}

			[[nodiscard]] Iterator begin() {
				sul::dynamic_bitset<>::size_type firstIndex = m_SlotStates.find_first();

				if (firstIndex == sul::dynamic_bitset<>::npos) {
					return end();
				}

				return Iterator(this, firstIndex);
			}

			[[nodiscard]] Iterator end() {
				return Iterator(this, static_cast<SizeT>(m_Data.Capacity()));
			}

			[[nodiscard]] ReverseIterator rbegin() {
				return ReverseIterator(end());
			}

			[[nodiscard]] ReverseIterator rend() {
				return ReverseIterator(begin());
			}

			[[nodiscard]] ConstIterator begin() const {
				sul::dynamic_bitset<>::size_type firstIndex = m_SlotStates.find_first();

				if (firstIndex == sul::dynamic_bitset<>::npos) {
					return end();
				}

				return ConstIterator(this, firstIndex);
			}

			[[nodiscard]] ConstIterator end() const {
				return ConstIterator(this, static_cast<SizeT>(m_Data.Capacity()));;
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

			[[nodiscard]] SizeT Size() const {
				return m_Data.Size() - m_Holes.size();
			}

			[[nodiscard]] SizeT RawSize() const {
				return m_Data.Size();
			}

			[[nodiscard]] SizeT Capacity() const {
				return m_Data.Capacity();
			}

			[[nodiscard]] bool Empty() const {
				return Size() == 0;
			}

			void Sync() {
				m_Data.Sync();
			}

			[[nodiscard]] VulkanBuffer& GetVulkanBuffer() {
				return m_Data.GetVulkanBuffer();
			}

			[[nodiscard]] const VulkanBuffer& GetVulkanBuffer() const {
				return m_Data.GetVulkanBuffer();
			}

			[[nodiscard]] bool IsHandleValid(const ConstHandle handle) const {
				if constexpr (ENABLE_VERSIONING) {
					return IsIndexValid(handle.GetIndex()) && m_Versions[handle.GetIndex()] == handle.GetVersion();
				}

				return IsIndexValid(handle.GetIndex()) && handle.GetVersion() == 1;
			}

			[[nodiscard]] bool IsIndexValid(const SizeT index) const {
				return index < RawSize() && m_SlotStates[index];
			}

			[[nodiscard]] bool IsIndexOccupied(const SizeT index) const {
				CORI_CORE_ASSERT(index < RawSize(), "VulkanFlatSlotMap: IsIndexOccupied index out of bounds.");

				return m_SlotStates[index];
			}

			[[nodiscard]] Handle GetIndexHandle(const SizeT index) const {
				CORI_CORE_ASSERT(IsIndexValid(index), "Invalid index passed to VulkanFlatSlotMap::GetIndexHandle.");

				if constexpr (ENABLE_VERSIONING) {
					return { index, m_Versions[index] };
				}

				return Handle{ index, 1 };
			}

			void Clear() {
				m_Data.Clear();
				if constexpr (ENABLE_VERSIONING) {
					m_Versions.clear();
				}
				m_Holes.clear();
				m_SlotStates.clear();
			}

		protected:
			friend Iterator;
			friend ConstIterator;

			VulkanDynamicVector<T> m_Data{};
			sul::dynamic_bitset<> m_SlotStates{};
		private:
			std::deque<SizeT> m_Holes{};
			std::conditional_t<ENABLE_VERSIONING, std::vector<uint32_t>, void> m_Versions{};
			uint32_t m_ReusedIndexCounter{ 0 };
		};

		static constexpr uint32_t TRANSFERS_IN_FLIGHT{ 3 };

		class VulkanStreamingLine {
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

			struct BufferUpload {
				VulkanBuffer resource;
				BufferUploadRange range;
				vk::PipelineStageFlagBits2 srcPipelineStages;
				vk::AccessFlagBits2 srcAccessFlags;
			};

			struct ImageUpload {
				VulkanImage resource;
				ImageUploadRange range;
				vk::ImageLayout dstLayout;
				uint32_t dstQueueFamilyIndex{};
			};
		private:
			struct Allocation {
				vk::DeviceSize offset{};
				vma::VirtualAllocation virtualAllocation;
			};

			struct PendingUpload {
				std::variant<ImageUpload, BufferUpload> resourceUpload;
				uint64_t stagingSize;
				Allocation stagingAllocation;
			};

			struct SlotData {
				std::vector<PendingUpload> pendingUploads;
				uint64_t completionTicket{ 0 };
				bool isBusy{ false };
				vk::CommandBuffer primaryCmb;
				vk::CommandBuffer secondaryCmb;
			};

		public:

			struct GenericUpload {
				std::variant<std::monostate, ImageUpload, BufferUpload> resourceUpload;
				std::span<const Byte> data;
			};

			static void Init();

			static void Shutdown();

			static VulkanStreamingLine& Get();

			[[nodiscard]] static std::expected<uint64_t, ErrorCode> SubmitUploads(const std::span<const GenericUpload>& uploads) {
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
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset '{}' provided when calling SubmitUpdate for resource VulkanBuffer '{}' is misaligned to the provided alignment of '{}', no upload will be made, this batch will be skipped.", bufferUpload.range.offset, bufferUpload.resource.m_Name, bufferUpload.range.alignment);
							#else
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::VirtualBuffer }, "Offset '{}' provided when calling SubmitUpdate for resource VulkanBuffer '{}' is misaligned to the provided alignment of '{}', no upload will be made, this batch will be skipped.", bufferUpload.range.offset, "Name is unavailable in release build", bufferUpload.range.alignment);
							#endif
							success = false;
							break;
						}

						if (bufferUpload.range.offset + data.size_bytes() > bufferUpload.resource.m_Size) {
							#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Trying to upload to VulkanBuffer '{}' via VulkanStreamingLine, but upload is out of bounds, offest '{}', data size '{}', buffer size '{}', no upload will be made, this batch will be skipped.", bufferUpload.resource.m_Name, bufferUpload.range.offset, data.size_bytes(), bufferUpload.resource.m_Size);
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
							success = false;
							break;
						}
					} else {
						auto& imageUpload = std::get<ImageUpload>(upload);

						if (imageUpload.range.subresourceLayers.baseArrayLayer + imageUpload.range.subresourceLayers.layerCount > imageUpload.resource.m_ArrayLayers) {
							#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid BaseArrayLayer '{}' and/or LayerCount '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, the sum of them can't be more than the total image layer count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.baseArrayLayer, imageUpload.range.subresourceLayers.layerCount, imageUpload.resource.m_Name, imageUpload.resource.m_ArrayLayers);
							#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid BaseArrayLayer '{}' and/or LayerCount '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, the sum of them can't be more than the total image layer count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.baseArrayLayer, imageUpload.range.subresourceLayers.layerCount, "Name is unavailable in release build", imageUpload.resource.m_ArrayLayers);
							#endif

							success = false;
							break;
						}

						if (imageUpload.range.subresourceLayers.mipLevel > imageUpload.resource.m_MipLevels) {
							#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid MipLevel '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, MipLevel can't be more than the total image mip level count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.mipLevel, imageUpload.resource.m_Name, imageUpload.resource.m_MipLevels);
							#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid MipLevel '{}' was provided when trying to upload to VulkanImage '{}' via streaming line, MipLevel can't be more than the total image mip level count '{}', no upload will be made, this batch will be skipped.", imageUpload.range.subresourceLayers.mipLevel, "Name is unavailable in release build", imageUpload.resource.m_MipLevels);
							#endif

							success = false;
							break;
						}

						if (imageUpload.range.extent.width == 0 || imageUpload.range.extent.height == 0 || imageUpload.range.extent.depth == 0) {
							#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid Extent3D '{} {} {}' provided when trying to upload to VulkanImage '{}' via streaming line, no extent part can be 0, no upload will be made, this batch will be skipped.", imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.resource.m_Name);
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
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset3D '{} {} {}' provided when calling SubmitUpdate for resource VulkanImage '{}' is misaligned to the block extent '{} {} {}' of image format '{}', no upload will be made, this batch will be skipped.", imageUpload.range.offset.x, imageUpload.range.offset.y, imageUpload.range.offset.z, imageUpload.resource.m_Name, blockExtent[0], blockExtent[1], blockExtent[2], vk::to_string(imageUpload.resource.m_Format));
							#else
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset3D '{} {} {}' provided when calling SubmitUpdate for resource VulkanImage '{}' is misaligned to the block extend '{} {} {}' of image format '{}', no upload will be made, this batch will be skipped.", imageUpload.range.offset.x, imageUpload.range.offset.y, imageUpload.range.offset.z, "Name is unavailable in release build", blockExtent[0], blockExtent[1], blockExtent[2], vk::to_string(imageUpload.resource.m_Format));
							#endif

							success = false;
							break;
						}

						if (imageUpload.range.offset.x + imageUpload.range.extent.width > mipExtent.width || imageUpload.range.offset.y + imageUpload.range.extent.height > mipExtent.height || imageUpload.range.offset.z + imageUpload.range.extent.depth > mipExtent.depth) {
							#ifdef DEBUG_BUILD
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Offset3D '{} {} {}' + Extent3D '{} {} {}' provided when calling SubmitUpdate for resource VulkanImage '{}' is located beyond the image mip level '{}' Extent3D '{} {} {}', no upload will be made, this batch will be skipped.", imageUpload.range.offset.x, imageUpload.range.offset.y, imageUpload.range.offset.z, imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.resource.m_Name, imageUpload.range.subresourceLayers.mipLevel, mipExtent.width, mipExtent.height, mipExtent.depth);
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
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Invalid Extent3D '{} {} {}' provided when trying to upload to VulkanImage '{}' via StreamingLine, extent + offset is not at the image border and extent is not aligned to block extent '{} {} {}' of image format '{}', no upload will be made, this batch will be skipped.", imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.resource.m_Name, blockExtent[0], blockExtent[1], blockExtent[2], vk::to_string(imageUpload.resource.m_Format));
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
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::StreamingLine }, "Data with invalid size was provided when trying to upload to VulkanImage '{}' via StreamingLine, upload Extent3D '{} {} {}', mip layer '{}', array layer count '{}', image format '{}', block size '{}', total expected data size '{}', provided data size '{}', no upload will be made, this batch will be skipped.", imageUpload.resource.m_Name, imageUpload.range.extent.width, imageUpload.range.extent.height, imageUpload.range.extent.depth, imageUpload.range.subresourceLayers.mipLevel, imageUpload.range.subresourceLayers.layerCount, vk::to_string(imageUpload.resource.m_Format), blockSize, expectedDataSize, data.size_bytes());
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

				for (auto [in, pending] : std::views::zip(uploads, std::ranges::subrange(slot.pendingUploads.begin() + startOffset, slot.pendingUploads.end()))) {
					auto result = VulkanEngine::GetAllocator().copyMemoryToAllocation(in.data.data(), Get().m_RingStagingBuffer.m_Allocation, pending.stagingAllocation.offset, pending.stagingSize);
					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy streaming data to the staging buffer. Error: {}", vk::to_string(result));
				}

				return Get().m_NextTicket;
			}

			[[nodiscard]] static bool CheckTicket(const uint64_t ticket) {
				auto [result, currentValue] = VulkanEngine::GetLogicalDevice().getSemaphoreCounterValue(Get().m_TimelineSemaphore);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to get value of the timeline semaphore in the streaming line. Error: {}", vk::to_string(result));

				return currentValue >= ticket;
			}

			[[nodiscard]] static vk::Semaphore GetTimelineSemaphoreHandle() {
				return Get().m_TimelineSemaphore;
			}

			[[nodiscard]] static uint64_t GetTimelineValue() {
				auto [result, currentValue] = VulkanEngine::GetLogicalDevice().getSemaphoreCounterValue(Get().m_TimelineSemaphore);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to get value of the timeline semaphore in the streaming line. Error: {}", vk::to_string(result));

				return currentValue;
			}

			[[nodiscard]] static std::expected<uint64_t, ErrorCode> SubmitUploads(const GenericUpload& upload) {
				return SubmitUploads(std::span{ &upload, 1 });
			}

			static void ProcessUploads() {
				for (auto& slot : Get().m_Slots) {
					if (!slot.pendingUploads.empty() && slot.isBusy == false) {

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

						for (auto& pending : slot.pendingUploads) {
							if (std::holds_alternative<ImageUpload>(pending.resourceUpload)) {
								auto& resourceUpload = std::get<ImageUpload>(pending.resourceUpload);

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

						result = slot.secondaryCmb.end();
						CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end secondary command buffer recording in VulkanStreamingLine. Error: {}", vk::to_string(result));

						vk::CommandBufferBeginInfo primaryBeginInfo {
							.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
						};

						result = slot.primaryCmb.begin(primaryBeginInfo);
						CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to begin primary command buffer recording in VulkanStreamingLine. Error: {}", vk::to_string(result));

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

						result = slot.primaryCmb.end();
						CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to end primary command buffer recording in VulkanStreamingLine. Error: {}", vk::to_string(result));

						vk::TimelineSemaphoreSubmitInfo timelineInfo {
							.signalSemaphoreValueCount = 1,
							.pSignalSemaphoreValues = &Get().m_NextTicket
						};

						vk::SubmitInfo submitInfo{
							.pNext = &timelineInfo,
							.commandBufferCount = 1,
							.pCommandBuffers = &slot.primaryCmb,
							.signalSemaphoreCount = 1,
							.pSignalSemaphores = &Get().m_TimelineSemaphore
						};

						result = VulkanEngine::GetTransferQueue().submit(1, &submitInfo, VK_NULL_HANDLE);
						CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Transfer queue submission failed in VulkanStreamingLine. Error: {}", vk::to_string(result));

						Get().m_BufferBarriersCache.clear();
						Get().m_AcquireImageBarriersCache.clear();
						Get().m_ReleaseImageBarriersCache.clear();

						slot.isBusy = true;
						slot.completionTicket = Get().m_NextTicket++;
						break;
					}
				}
			}

			~VulkanStreamingLine() {
				m_StagingBlock.clearVirtualBlock();
				m_StagingBlock.destroy();

				m_RingStagingBuffer.Destroy();

				for (auto& slot : m_Slots) {
					VulkanEngine::GetLogicalDevice().freeCommandBuffers(VulkanEngine::GetTransferCmp(), slot.primaryCmb);
					VulkanEngine::GetLogicalDevice().freeCommandBuffers(VulkanEngine::GetTransferCmp(), slot.secondaryCmb);
				}

				VulkanEngine::GetLogicalDevice().destroySemaphore(m_TimelineSemaphore);
			}

		private:
			VulkanStreamingLine() {
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
					.usage = vma::MemoryUsage::eAuto
				};

				VulkanBuffer::CreateInfo stagingInfo {
					.bufferCreateInfo = &stagingCreateInfo,
					.allocationCreateInfo = &allocCreateInfo,
				};

				m_RingStagingBuffer = VulkanBuffer::Create(stagingInfo);

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

			[[nodiscard]] std::optional<uint32_t> GetFreeTransferSlot() {
				uint32_t freeSlot = UINT32_MAX;
				uint32_t slotCounter = 0;
				auto [result, currentValue] = VulkanEngine::GetLogicalDevice().getSemaphoreCounterValue(Get().m_TimelineSemaphore);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to get value of the timeline semaphore in the streaming line. Error: {}", vk::to_string(result));

				for (const auto& slot : m_Slots) {
					if (currentValue >= slot.completionTicket) {
						if (freeSlot == UINT32_MAX) {
							freeSlot = slotCounter;
						}

						if (slot.isBusy == true) {
							ScrubSlot(slotCounter);
						}
					}

					slotCounter++;
				}

				if (freeSlot != UINT32_MAX) {
					return freeSlot;
				}

				return std::nullopt;
			}

			void ScrubSlot(const uint32_t slotIndex) {
				auto& slot = m_Slots[slotIndex];
				slot.isBusy = false;

				for (auto& pending : slot.pendingUploads) {
					Get().m_StagingBlock.free(pending.stagingAllocation.virtualAllocation);
				}

				slot.pendingUploads.clear();
			}

			[[nodiscard]] std::optional<Allocation> TryAllocate(const uint64_t size, const uint64_t alignment) {
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

			vma::VirtualBlock m_StagingBlock;
			VulkanBuffer m_RingStagingBuffer;

			std::array<SlotData, TRANSFERS_IN_FLIGHT> m_Slots;

			std::vector<vk::BufferMemoryBarrier2> m_BufferBarriersCache;

			std::vector<vk::ImageMemoryBarrier2> m_AcquireImageBarriersCache;
			std::vector<vk::ImageMemoryBarrier2> m_ReleaseImageBarriersCache;
			vk::Semaphore m_TimelineSemaphore;

			uint64_t m_NextTicket{ 1 };

			static constexpr uint64_t HEAP_SIZE{ 128 * 1024 * 1024 };

			static std::unique_ptr<VulkanStreamingLine> s_Instance;

		};
	}
}