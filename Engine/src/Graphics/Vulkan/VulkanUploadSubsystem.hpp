#pragma once
#include <sul/dynamic_bitset.hpp>
#include "VulkanEngine.hpp"
#include "VulkanBuffer.hpp"

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
				if ((alignof(T) > m_Alignment || m_Alignment % alignof(T) != 0) && CORI_DEBUG_BOOL) {
					//FIXME: error critical misalignment
					return;
				}

				if (m_Alignment % offset != 0) {
					offset = Math::AlignUp(offset, m_Alignment);
					//FIXME: warn, offset misalignment can cause errors
				}

				if ((offset + data.size_bytes() > m_Size) && CORI_DEBUG_BOOL) {
					//FIXME: error message here
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

		private:
			friend class VulkanVirtualBufferAllocator;
			VulkanVirtualBuffer() = default;

			Type m_Type;
			vk::Buffer m_Heap;
			vma::Allocation m_Alloc;
			uint64_t m_StartBDA{ 0 };
			uint64_t m_Size{ 0 };
			uint64_t m_StartOffset{ 0 };
			uint64_t m_Alignment{ 0 };
			const char* m_Name;
			static std::function<void(const VulkanVirtualBuffer&)> s_UploadListener;
		};

		class VulkanVirtualBufferAllocator {
		public:
			~VulkanVirtualBufferAllocator() {

			}

			static VulkanVirtualBuffer CreateVirtualUploadBuffer(uint64_t size, const uint64_t alignment, const char* name = "") {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
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

				virtBuff.m_StartBDA = Get().m_GPUScratchBDA + virtBuff.m_StartOffset;
				virtBuff.m_Type = VulkanVirtualBuffer::Type::CPUUpload;
				virtBuff.m_Name = name;
				virtBuff.m_Alignment = alignment;
				Get().m_DestructionQueue[frameIndex].first.emplace_back(alloc);
				return virtBuff;
			}

			static VulkanVirtualBuffer CreateVirtualScratchBuffer(uint64_t size, const uint64_t alignment, const char* name = "") {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
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
				virtBuff.m_Name = name;
				virtBuff.m_Alignment = alignment;
				Get().m_DestructionQueue[frameIndex].second.emplace_back(alloc);
				return virtBuff;
			}

			static void ScrubHeaps() {
				uint32_t prevFrameIndex = VulkanEngine::GetPreviousFrameInFlight();
				for (auto [upload, scratch] : std::views::zip(Get().m_DestructionQueue[prevFrameIndex].first, Get().m_DestructionQueue[prevFrameIndex].second) | std::views::reverse) {
					Get().m_UploadArenaBlock.free(upload);
					Get().m_GPUScratchBlock.free(scratch);
				}
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
					//FIXME: raise a warn

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

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_DestructionQueue[i].first.reserve(16);
					m_DestructionQueue[i].second.reserve(16);
				}

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
				m_UploadArenaBlock = vBlock_;
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

			std::array<std::pair<std::vector<vma::VirtualAllocation>, std::vector<vma::VirtualAllocation>>, FRAMES_IN_FLIGHT> m_DestructionQueue;

			static std::unique_ptr<VulkanVirtualBufferAllocator> s_Instance;
		};

		class VulkanDynamicContainerUploadManager {
		public:
			~VulkanDynamicContainerUploadManager() {

			}

			static void Init();

			static void Shutdown();

			static VulkanDynamicContainerUploadManager& Get();

			static void ProcessUpdates() {

			}

		protected:
			template <typename T> friend class VulkanDynamicVector;
			static void Upload(void* data, const uint64_t size, const uint64_t offset, vk::Buffer dstBuffer) {

			}

			static void FallbackListener() {

			}

		private:
			VulkanDynamicContainerUploadManager() = default;

			static constexpr uint64_t STAGING_SIZE{ 192 * 1024 * 1024 };

			VulkanBuffer m_RingStagingBuffer;
			vma::VirtualBlock m_RingStagingBlock;

			std::array<std::vector<vma::VirtualAllocation>, FRAMES_IN_FLIGHT> m_DestructionQueue;

			bool m_StagingBufferCreated{ false };

			static std::unique_ptr<VulkanDynamicContainerUploadManager> s_Instance;
		};

		template <typename T>
		class VulkanDynamicVector {
			struct PendingReallocation {
				VulkanBuffer srcBuffer;
				uint64_t dataSize;
				bool active{ false };
			};
		public:

			using iterator = std::vector<T>::iterator;
			using const_iterator = std::vector<T>::const_iterator;

			VulkanDynamicVector() {

			}

			~VulkanDynamicVector() {

			}

			VulkanDynamicVector(const VulkanDynamicVector&) = delete;
			VulkanDynamicVector& operator=(const VulkanDynamicVector&) = delete;

			VulkanDynamicVector(VulkanDynamicVector&& other) {

			}

			VulkanDynamicVector& operator=(VulkanDynamicVector&& other) {

			}

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
					size_t newCap = m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * 2;
					Reserve(newCap);
				}

				uint64_t index = m_CPUShadow.size();
				m_CPUShadow.push_back(value);

				ReportChange(index * sizeof(T), sizeof(T));
			}

			template <typename... Args>
			void EmplaceBack(Args&&... args) {
				if (m_CPUShadow.size() == m_CPUShadow.capacity()) {
					uint64_t newCap = m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * 2;
					Reserve(newCap);
				}

				uint64_t index = m_CPUShadow.size();
				m_CPUShadow.emplace_back(std::forward<Args>(args)...);

				ReportChange(index * sizeof(T), sizeof(T));
			}


			void PopBack() {
				if (!m_CPUShadow.empty()) {
					m_CPUShadow.pop_back();
				}
			}

			const T& Back() const {
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Back on empty VulkanDynamicVector.")
				return m_CPUShadow.back();
			}

			T& Back() {
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Back on empty VulkanDynamicVector.")
				ReportChange((m_CPUShadow.size() - 1) * sizeof(T), sizeof(T));
				return m_CPUShadow.back();
			}

			const T& Front() const {
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Front on empty VulkanDynamicVector.")
				return m_CPUShadow.front();
			}

			T& Front() {
				CORI_CORE_ASSERT(m_CPUShadow.empty(), "Calling Front on empty VulkanDynamicVector.")
				ReportChange(0, sizeof(T));
				return m_CPUShadow.front();
			}

			void Clear() {
				m_CPUShadow.clear();
			}

			//typical vector methods

			[[nodiscard]] uint64_t Size() const {
				return m_CPUShadow.size();
			}

			[[nodiscard]] uint64_t Capacity() const {
				return m_CPUShadow.capacity();
			}

			[[nodiscard]] bool Empty() const {
				return m_CPUShadow.empty();
			}

			[[nodiscard]] const T& operator[](const uint64_t index) const {
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector index '{}' out of bounds.", index);
				return m_CPUShadow[index];
			}

			[[nodiscard]] T& operator[](const uint64_t index) {
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector index '{}' out of bounds.", index);
				ReportChange(index * sizeof(T), sizeof(T));
				return m_CPUShadow[index];
			}

			[[nodiscard]] const T& At(const uint64_t index) const {
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector index '{}' out of bounds.", index);
				return m_CPUShadow.at(index);
			}

			[[nodiscard]] T& At(const uint64_t index) {
				CORI_CORE_ASSERT(index < m_CPUShadow.size(), "VulkanDynamicVector index '{}' out of bounds.", index);
				ReportChange(index * sizeof(T), sizeof(T));
				return m_CPUShadow.at(index);
			}

			void Sync() {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
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
						VulkanDynamicContainerUploadManager::Upload(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_CPUShadow.data()) + offset), size, offset, m_GPUBuffers[frameIndex].m_Buffer);
					}

					currentRangeBeginning = m_SectorStates[frameIndex].find_next(currentRangeEnd);
				}

				m_SectorStates[frameIndex].reset();
			}

			void Reserve(const uint64_t newCapacity) {
				if (newCapacity <= m_CPUShadow.capacity()) {
					return;
				}

				ResizeBuffers(newCapacity);

				m_CPUShadow.reserve(newCapacity);
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

			std::vector<T>::iterator begin() { return m_CPUShadow.begin(); }
			std::vector<T>::iterator end() { return m_CPUShadow.end(); }
			std::vector<T>::const_iterator begin() const { return m_CPUShadow.begin(); }
			std::vector<T>::const_iterator end() const { return m_CPUShadow.end(); }

			std::vector<T>::iterator rbegin() { return m_CPUShadow.rbegin(); }
			std::vector<T>::iterator rend() { return m_CPUShadow.rend(); }
			std::vector<T>::const_iterator rbegin() const { return m_CPUShadow.rbegin(); }
			std::vector<T>::const_iterator rend() const { return m_CPUShadow.rend(); }


		private:
			void ReportChange(const uint64_t startOffset, const uint64_t size) {
				uint32_t affectedSectorStart = floor(startOffset / SECTOR_SIZE);
				uint32_t affectedSectorEnd = floor((startOffset + size - 1) / SECTOR_SIZE);

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_SectorStates[i].set(affectedSectorStart, affectedSectorEnd - affectedSectorStart + 1, true);
				}
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
				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_SectorStates[i].resize(newSize);
				}

				vk::BufferCreateInfo bufferCreateInfo{
					.size = newSize,
					.usage = m_UsageFlags,
					.sharingMode = m_SharingMode,
					.queueFamilyIndexCount = static_cast<uint32_t>(m_QueueFamilyIndices.size()),
					.pQueueFamilyIndices = m_QueueFamilyIndices.data()
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

					std::string name = std::format("{} GPU buffer {}", m_Name, i);

					VulkanBuffer::CreateInfo info {
						.bufferCreateInfo = &bufferCreateInfo,
						.allocationCreateInfo = &BARAlloc,
						.name = name.c_str()
					};

					m_PendingReallocations[i].srcBuffer = m_GPUBuffers[i];
					m_PendingReallocations[i].dataSize = sizeof(T) * m_CPUShadow.size();
					m_PendingReallocations[i].active = true;

					m_GPUBuffers[i] = VulkanBuffer::Create(info);
				}

				std::array<bool, FRAMES_IN_FLIGHT> isBufferBAR{ true };
				bool BAR = true;

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					auto propertyFlags = VulkanEngine::GetAllocator().getAllocationMemoryProperties(m_GPUBuffers[i].m_Allocation);
					if (!(propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)) {
						isBufferBAR[i] = false;
						BAR = false;
					}
				}

				if (!BAR) {
					VulkanDynamicContainerUploadManager::FallbackListener();
					for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
						std::string name = std::format("{} GPU buffer {}", m_Name, i);

						VulkanBuffer::CreateInfo info {
							.bufferCreateInfo = &bufferCreateInfo,
							.allocationCreateInfo = &localAlloc,
							.name = name.c_str()
						};

						if (isBufferBAR[i]) {
							m_GPUBuffers[i].Destroy();

							m_GPUBuffers[i] = VulkanBuffer::Create(info);
						}
					}
				}
			}

			std::vector<T> m_CPUShadow;
			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_GPUBuffers;
			std::array<sul::dynamic_bitset<>, FRAMES_IN_FLIGHT> m_SectorStates;
			std::array<PendingReallocation, FRAMES_IN_FLIGHT> m_PendingReallocations;
			std::vector<uint32_t> m_QueueFamilyIndices;
			vk::BufferUsageFlags m_UsageFlags;
			vk::SharingMode m_SharingMode;
			const char* m_Name{ "" };
			float m_LastSectorSize{ SECTOR_SIZE };
			bool m_EvenSectors{ false };
			bool m_IsBAR{ false };

			static constexpr float SECTOR_SIZE{ 4 * 1024 }; //4kb

		};


	}
}