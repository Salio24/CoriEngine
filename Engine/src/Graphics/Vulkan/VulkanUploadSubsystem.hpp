#pragma once
#include <sul/dynamic_bitset.hpp>
#include <utility>
#include "VulkanEngine.hpp"
#include "VulkanBuffer.hpp"
#include "DeletionQueue.hpp"
#include "VmaLeakLog.hpp"
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
			~VulkanVirtualBufferAllocator();

			static VulkanVirtualBuffer CreateVirtualUploadBuffer(uint64_t size, uint64_t alignment, uint32_t dstFrameInFlight, const char* name = "");

			static VulkanVirtualBuffer CreateVirtualScratchBuffer(uint64_t size, uint64_t alignment, uint32_t dstFrameInFlight, const char* name = "");

			static void ClearGPUScratchBlock(uint32_t frameInFlight);

			static void SubmitCopies(vk::CommandBuffer cmb);

			static void Init();

			static void Shutdown();

			static VulkanVirtualBufferAllocator& Get();

		protected:
			friend VulkanVirtualBuffer;

			void ProcessUpload(const VulkanVirtualBuffer& buffer);

		private:
			VulkanVirtualBufferAllocator();

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
			~VulkanDynamicContainerUploadManager();

			static void Init();

			static void Shutdown();

			static VulkanDynamicContainerUploadManager& Get();

			static void ProcessUpdates(vk::CommandBuffer cmb);

		protected:
			template <Utility::NotBool T> friend class VulkanDynamicVector;
			template <typename T, QueueUsageFlags QUEUE_USAGE, vk::BufferUsageFlags BUFFER_USAGE, glz::string_literal NAME, typename Allocator> friend class VulkanGPUSyncedSequentialStorage;

			static void BeginUpdate(vk::Buffer dstBuffer);

			static void Upload(const void* data, uint64_t size, uint64_t offset);

			static void EndUpdate();

			static void RecordCopy(VulkanBuffer& srcBuffer, vk::Buffer dstBuffer, uint64_t size);

			static void FallbackListener();

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
				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					if (m_PendingReallocations[i].active) {
						DeletionQueue::PushBuffer(m_PendingReallocations[i].srcBuffer, i);
					}

					if (m_GPUBuffers[i].m_Buffer) {
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
				using ValueType = std::vector<T>::iterator::ValueType;
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

			void InsertRange(const std::span<T>& data, uint64_t offset) {
				if (offset + data.size() >= m_CPUShadow.capacity()) {
					uint64_t newCap = std::max(m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * 2, m_CPUShadow.capacity() + data.size());
					Reserve(newCap);
				}

				if (offset + data.size() >= m_CPUShadow.size()) {
					ResizeNonReporting(offset + data.size());
				}

				ReportChange(offset * sizeof(T), data.size_bytes());
				memcpy(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_CPUShadow.data()) + offset * sizeof(T)), data.data(), data.size_bytes());
			}

			void AppendRange(const std::span<T>& data) {
				uint64_t changeStart = m_CPUShadow.size();
				if (changeStart + data.size() >= m_CPUShadow.capacity()) {
					uint64_t newCap = std::max(m_CPUShadow.capacity() == 0 ? 4 : m_CPUShadow.capacity() * 2, m_CPUShadow.capacity() + data.size());
					Reserve(newCap);
				}

				ResizeNonReporting(changeStart + data.size());

				ReportChange(changeStart * sizeof(T), data.size_bytes());
				memcpy(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_CPUShadow.data()) + changeStart * sizeof(T)), data.data(), data.size_bytes());
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
						const uint64_t lastSector = m_SectorStates[frameIndex].size() - 1;
						uint64_t size = (currentRangeEnd - currentRangeBeginning) * SECTOR_SIZE + (currentRangeEnd == lastSector ? m_LastSectorSize : SECTOR_SIZE);

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

				#ifdef CORI_VALIDATION_LAYER
				VerifyGPUMirror();
				#endif
			}

			void VerifyGPUMirror() {
				#ifdef CORI_VALIDATION_LAYER
				const uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();

				if (!m_IsBAR || m_CPUShadow.empty() || !m_GPUBuffers[frameIndex].m_Buffer) {
					return;
				}

				const uint64_t byteSize = m_CPUShadow.size() * sizeof(T);

				std::vector<Byte> readback(byteSize);
				auto result = VulkanEngine::GetAllocator().copyAllocationToMemory(m_GPUBuffers[frameIndex].m_Allocation, 0, readback.data(), byteSize);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "VulkanDynamicVector '{}' failed to read back its GPU buffer for verification. Error: {}", m_Name, vk::to_string(result));

				const auto* cpuBytes = reinterpret_cast<const Byte*>(m_CPUShadow.data());

				if (memcmp(readback.data(), cpuBytes, byteSize) == 0) {
					return;
				}

				uint64_t firstDiff = 0;
				while (firstDiff < byteSize && readback[firstDiff] == cpuBytes[firstDiff]) {
					firstDiff++;
				}

				CORI_CORE_ASSERT(false, "VulkanDynamicVector '{}' GPU buffer diverged from CPU shadow on frame {}: first mismatch at byte {} (element {}, sector {} of {}), CPU=0x{:02x} GPU=0x{:02x}. The dirty sector flush missed this write.", m_Name, frameIndex, firstDiff, firstDiff / sizeof(T), firstDiff / SECTOR_SIZE, m_SectorStates[frameIndex].size(), cpuBytes[firstDiff], readback[firstDiff]);
				#endif
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
		private:

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
						m_PendingReallocations[i].size = std::min<uint64_t>(sizeof(T) * m_OldSize, m_GPUBuffers[i].m_Size);
						m_PendingReallocations[i].active = true;
					}

					#ifdef DEBUG_BUILD
					std::string name = std::format("{} GPU buffer {}", m_Name, i);
					info.name = name.c_str();
					#endif

					m_GPUBuffers[i] = VulkanBuffer::Create(info);
				}

				std::array<bool, FRAMES_IN_FLIGHT> isBufferBAR{};
				isBufferBAR.fill(true);
				m_IsBAR = true;

				const uint64_t totalBytes = newSize * sizeof(T);
				uint32_t lastSectorSize = totalBytes % SECTOR_SIZE;
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

		template<typename T, uint16_t REUSE_THRESHOLD = 64, bool ENABLE_VERSIONING = true, Core::IsVersionedHandle HandleT = Core::Handle<T>, typename ConstHandleT = Core::ConstHandle<T>> requires std::derived_from<HandleT, ConstHandleT>
		class VulkanFlatSlotMap {
		public:
			using Handle = HandleT;
			using ConstHandle = ConstHandleT;

			using SizeT = uint32_t;

			template<bool IsConst>
			struct IteratorImpl {
				using iterator_category = std::bidirectional_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using ValueType = T;
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

				[[nodiscard]] SizeT GetIndex() const {
					return m_Index;
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

					new (&m_Data[index]) T(std::forward<Args>(args)...);

					//m_Data[index] = std::move(T(std::forward<Args>(args)...));

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

				m_OccupancyCounter++;

				if constexpr (ENABLE_VERSIONING) {
					return Handle{ index, m_Versions[index] };
				}

				return Handle{ index, 1 };
			}

			template<typename... Args>
			bool EmplaceAt(const SizeT index, Args&&... args) {
				static_assert(ENABLE_VERSIONING == false, "Index version of EmplaceAt should only be used with versioning turned off.");

				// turned out i actually need pseudo sparse container capabilities here. Not idea as we construct all items inbetween the end and the index, but to fix that i need to use raw memory instead of a vector.
				// but, regardless its not that common.
				//if (index > RawSize()) {
				//	if (index >= Capacity()) {
				//		SizeT newSize = m_Data.Capacity() == 0 ? 4 : m_Data.Capacity() * 2.0f;
				//		m_Data.Reserve(newSize);
				//		m_SlotStates.reserve(newSize);
				//	}
				//
				//	m_Data.ResizeNonReporting(index);
				//	m_SlotStates.resize(index);
				//}
				//i need true sparse map (((

				if (index == RawSize()) {
					m_Data.EmplaceBack(std::forward<Args>(args)...);
					m_SlotStates.push_back(true);
				} else {
					CORI_CORE_ASSERT(!m_SlotStates[index], "Double emplace at the same index {}", index);

					new (&m_Data[index]) T(std::forward<Args>(args)...);
					m_SlotStates[index] = true;
				}

				if constexpr (requires { m_Data[index].valid = bool{}; }) {
					m_Data[index].valid = true;
				}

				m_OccupancyCounter++;
				return true;
			}

			void Remove(const Handle handle) {
				if (!IsHandleValid(handle)) {
					return;
				}

				SizeT index = handle.GetIndex();

				auto& obj = m_Data[index];
				obj = {};

				m_SlotStates[index] = false;
				m_Holes.emplace_back(index);
				m_OccupancyCounter--;
			}

			void RemoveAt(const SizeT index) {
				static_assert(ENABLE_VERSIONING == false, "Index version of RemoveAt should only be used with versioning turned off.");

				if (!IsIndexValid(index)) {
					return;
				}

				auto& obj = m_Data[index];
				obj = {};

				m_SlotStates[index] = false;
				m_OccupancyCounter--;
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
				return m_OccupancyCounter;
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

				return IsIndexValid(handle.GetIndex());
			}

			[[nodiscard]] bool IsIndexValid(const SizeT index) const {
				return index < RawSize() && m_SlotStates[index];
			}

			[[nodiscard]] bool IsIndexOccupied(const SizeT index) const {
				if (index >= RawSize()) {
					return false;
				}

				return m_SlotStates[index];
			}

			[[nodiscard]] Handle GetIndexHandle(const SizeT index) const {
				static_assert(ENABLE_VERSIONING == true, "GetIndexHandle should only be used with versioning turned on.");
				CORI_CORE_ASSERT(IsIndexValid(index), "Invalid index passed to VulkanFlatSlotMap::GetIndexHandle.");
				return { index, m_Versions[index] };
			}

			void Clear() {
				m_Data.Clear();
				if constexpr (ENABLE_VERSIONING) {
					m_Versions.clear();
				}
				m_Holes.clear();
				m_SlotStates.clear();
				m_OccupancyCounter = 0;
			}

		protected:
			friend Iterator;
			friend ConstIterator;

			VulkanDynamicVector<T> m_Data{};
			sul::dynamic_bitset<> m_SlotStates{};
			SizeT m_OccupancyCounter{ 0 };
		private:
			std::deque<SizeT> m_Holes{};
			std::conditional_t<ENABLE_VERSIONING, std::vector<uint32_t>, uint8_t> m_Versions{};
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

			[[nodiscard]] static std::expected<uint64_t, ErrorCode> SubmitUploads(const std::span<const GenericUpload>& uploads);

			[[nodiscard]] static bool CheckTicket(uint64_t ticket);

			[[nodiscard]] static vk::Semaphore GetTimelineSemaphoreHandle();

			[[nodiscard]] static uint64_t GetTimelineValue();

			[[nodiscard]] static std::expected<uint64_t, ErrorCode> SubmitUploads(const GenericUpload& upload);

			static void ProcessUploads();

			~VulkanStreamingLine();

		private:
			VulkanStreamingLine();

			[[nodiscard]] std::optional<uint32_t> GetFreeTransferSlot();

			void ScrubSlot(uint32_t slotIndex);

			[[nodiscard]] std::optional<Allocation> TryAllocate(uint64_t size, uint64_t alignment);

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

		template <typename T, QueueUsageFlags QUEUE_USAGE, vk::BufferUsageFlags BUFFER_USAGE, glz::string_literal NAME, typename Allocator = std::allocator<T>>
		class VulkanGPUSyncedSequentialStorage {
			struct PendingReallocation {
				VulkanBuffer srcBuffer;
				uint64_t size{};
				bool active{ false };
			};
		public:
			using ValueType = T;
			using SizeType = std::allocator_traits<Allocator>::size_type;

			using Reference = T&;
			using ConstReference = const T&;

			VulkanGPUSyncedSequentialStorage() = default;

			explicit VulkanGPUSyncedSequentialStorage(const SizeType capacity) {
				ReallocateNonReporting(capacity);
			}

			~VulkanGPUSyncedSequentialStorage() {
				ReleaseResources();
			}

			VulkanGPUSyncedSequentialStorage(const VulkanGPUSyncedSequentialStorage&) = delete;
			VulkanGPUSyncedSequentialStorage& operator=(const VulkanGPUSyncedSequentialStorage&) = delete;

			VulkanGPUSyncedSequentialStorage(VulkanGPUSyncedSequentialStorage&& other) noexcept {
				Steal(std::move(other));
			}

			VulkanGPUSyncedSequentialStorage& operator=(VulkanGPUSyncedSequentialStorage&& other) noexcept {
				if (this == &other) {
					return *this;
				}

				ReleaseResources();
				Steal(std::move(other));

				return *this;
			}

			[[nodiscard]] ConstReference operator[](const SizeType index) const {
				CORI_CORE_ASSERT(index < m_Capacity, "VulkanGPUSyncedSequentialStorage '{}' index '{}' out of bounds.", NAME.sv(), index);
				return m_Data[index];
			}

			[[nodiscard]] Reference operator[](const SizeType index) {
				CORI_CORE_ASSERT(index < m_Capacity, "VulkanGPUSyncedSequentialStorage '{}' index '{}' out of bounds.", NAME.sv(), index);
				ReportChange(index * sizeof(T), sizeof(T));
				return m_Data[index];
			}

			[[nodiscard]] T* Data() {
				return m_Data;
			}

			[[nodiscard]] const T* Data() const {
				return m_Data;
			}

			[[nodiscard]] SizeType Capacity() const {
				return m_Capacity;
			}

			void Sync() {
				if (m_ResizeRequired) {
					ResizeBuffers(m_Capacity);
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
						const uint64_t lastSector = m_SectorStates[frameIndex].size() - 1;
						uint64_t size = (currentRangeEnd - currentRangeBeginning) * SECTOR_SIZE + (currentRangeEnd == lastSector ? m_LastSectorSize : SECTOR_SIZE);

						if (m_IsBAR) {
							auto result = VulkanEngine::GetAllocator().copyMemoryToAllocation(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_Data) + offset), m_GPUBuffers[frameIndex].m_Allocation, offset, size);
							CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to copy upload data to allocation of VulkanGPUSyncedSequentialStorage. Error: {}", vk::to_string(result));
						} else {
							VulkanDynamicContainerUploadManager::Upload(reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m_Data) + offset), size, offset);
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

			void Reallocate(const SizeType newCapacity) {
				ReallocateNonReporting(newCapacity);

				if (m_Capacity != 0) {
					ReportChange(0, m_Capacity * sizeof(T));
				}
			}

			void ReallocateNonReporting(const SizeType newCapacity) {
				if (newCapacity == m_Capacity) {
					return;
				}

				T* newData = newCapacity != 0 ? std::allocator_traits<Allocator>::allocate(m_Allocator, newCapacity) : nullptr;

				const SizeType carriedOverCount = std::min(m_Capacity, newCapacity);

				if (m_Data) {
					if (carriedOverCount != 0) {
						memcpy(newData, m_Data, carriedOverCount * sizeof(T));
					}

					std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
				}

				m_Data = newData;
				m_Capacity = newCapacity;
				m_OldSize = carriedOverCount;

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_SectorStates[i].resize(std::ceil(static_cast<float>(newCapacity * sizeof(T)) / static_cast<float>(SECTOR_SIZE)), false);
				}

				m_ResizeRequired = newCapacity != 0;

				if (newCapacity == 0) {
					ReleaseGPUBuffers();
					return;
				}

				PoisonRange(carriedOverCount, newCapacity - carriedOverCount);
			}

			void PoisonRange([[maybe_unused]] const SizeType start, [[maybe_unused]] const SizeType count) {
				#ifdef DEBUG_BUILD
				if (count == 0) {
					return;
				}

				CORI_CORE_ASSERT(start + count <= m_Capacity, "Range '{}' to '{}' passed to PoisonRange of VulkanGPUSyncedSequentialStorage '{}' is out of the storage bounds of '{}' elements.", start, start + count, NAME.sv(), m_Capacity);

				const uint64_t startByte = start * sizeof(T);
				const uint64_t byteCount = count * sizeof(T);

				auto* bytes = reinterpret_cast<Byte*>(m_Data);
				const auto* pattern = reinterpret_cast<const Byte*>(&s_PoisonValue);

				for (uint64_t i = startByte; i < startByte + byteCount; i++) {
					bytes[i] = pattern[i % sizeof(s_PoisonValue)];
				}

				ReportChange(startByte, byteCount);
				#endif
			}

			void ReportChange(const uint64_t startOffset, const uint64_t size) {
				uint32_t affectedSectorStart = std::floor(startOffset / SECTOR_SIZE);
				uint32_t affectedSectorEnd = std::floor((startOffset + size - 1) / SECTOR_SIZE);

				CORI_CORE_ASSERT(size != 0 && affectedSectorEnd < m_SectorStates[0].size(), "Change reported to VulkanGPUSyncedSequentialStorage '{}' at offset '{}' with size '{}' is out of the storage bounds.", NAME.sv(), startOffset, size);

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_SectorStates[i].set(affectedSectorStart, affectedSectorEnd - affectedSectorStart + 1, true);
				}

				m_IsDirtyMask = UINT8_MAX;
			}

			[[nodiscard]] VulkanBuffer& GetVulkanBuffer() {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				return m_GPUBuffers[frameIndex];
			}

			[[nodiscard]] const VulkanBuffer& GetVulkanBuffer() const {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				return m_GPUBuffers[frameIndex];
			}

		private:

			void ResizeBuffers(const uint64_t newSize) {
				auto& sharingSettings = VulkanEngine::GetBufferSharingSettings(QUEUE_USAGE);
				vk::BufferCreateInfo bufferCreateInfo{
					.size = newSize * sizeof(T),
					.usage = BUFFER_USAGE | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
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
						m_PendingReallocations[i].size = std::min<uint64_t>(sizeof(T) * m_OldSize, m_GPUBuffers[i].m_Size);
						m_PendingReallocations[i].active = true;
					}

					#ifdef DEBUG_BUILD
					std::string name = std::format("{} GPU buffer {}", NAME.sv(), i);
					info.name = name.c_str();
					#endif

					m_GPUBuffers[i] = VulkanBuffer::Create(info);
				}

				std::array<bool, FRAMES_IN_FLIGHT> isBufferBAR{};
				isBufferBAR.fill(true);
				m_IsBAR = true;

				const uint64_t totalBytes = newSize * sizeof(T);
				uint32_t lastSectorSize = totalBytes % SECTOR_SIZE;
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
							std::string name = std::format("{} GPU buffer {}", NAME.sv(), i);
							info.name = name.c_str();
							#endif

							m_GPUBuffers[i] = VulkanBuffer::Create(info);
						}
					}
				}
			}

			void ReleaseGPUBuffers() {
				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					if (m_PendingReallocations[i].active) {
						DeletionQueue::PushBuffer(m_PendingReallocations[i].srcBuffer, i);
						m_PendingReallocations[i] = PendingReallocation{};
					}

					if (m_GPUBuffers[i].m_Buffer) {
						DeletionQueue::PushBuffer(m_GPUBuffers[i], i);
						m_GPUBuffers[i] = VulkanBuffer{};
					}
				}

				m_LastSectorSize = SECTOR_SIZE;
				m_IsBAR = false;
				m_IsDirtyMask = 0;
			}

			void ReleaseResources() {
				ReleaseGPUBuffers();

				if (m_Data) {
					std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
				}
			}

			void Steal(VulkanGPUSyncedSequentialStorage&& other) noexcept {
				m_Allocator = std::move(other.m_Allocator);
				m_Data = other.m_Data;
				other.m_Data = nullptr;
				m_Capacity = other.m_Capacity;
				other.m_Capacity = 0;

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_GPUBuffers[i] = other.m_GPUBuffers[i];
					other.m_GPUBuffers[i] = VulkanBuffer{};
					m_SectorStates[i] = std::move(other.m_SectorStates[i]);
					m_PendingReallocations[i] = other.m_PendingReallocations[i];
					other.m_PendingReallocations[i] = PendingReallocation{};
				}

				m_LastSectorSize = other.m_LastSectorSize;
				other.m_LastSectorSize = SECTOR_SIZE;
				m_IsBAR = other.m_IsBAR;
				other.m_IsBAR = false;
				m_IsDirtyMask = other.m_IsDirtyMask;
				other.m_IsDirtyMask = 0;
				m_ResizeRequired = other.m_ResizeRequired;
				other.m_ResizeRequired = false;
				m_OldSize = other.m_OldSize;
				other.m_OldSize = 0;
			}

			[[no_unique_address]] Allocator m_Allocator{};
			T* m_Data{ nullptr };
			SizeType m_Capacity{ 0 };
			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_GPUBuffers;
			std::array<sul::dynamic_bitset<>, FRAMES_IN_FLIGHT> m_SectorStates;
			std::array<PendingReallocation, FRAMES_IN_FLIGHT> m_PendingReallocations;
			uint32_t m_LastSectorSize{ SECTOR_SIZE };
			bool m_IsBAR{ false };
			uint8_t m_IsDirtyMask{ 0 };
			bool m_ResizeRequired{ false };
			uint64_t m_OldSize{ 0 };

			static constexpr uint32_t SECTOR_SIZE{ 4 * 1024 }; //4kb
		};


	}
}