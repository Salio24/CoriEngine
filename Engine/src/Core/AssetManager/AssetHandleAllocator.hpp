#pragma once

namespace Cori {
	namespace Core {
		template<typename T, uint64_t BLOCK_COUNT, uint64_t BLOCK_SIZE>
		class AtomicSlotArray {
			static_assert(BLOCK_COUNT > 0, "Block count must be greater than 0");
			static_assert(BLOCK_SIZE > 0, "Block size must be greater than 0");
			static_assert(std::atomic<T>::is_always_lock_free, "AtomicSlotArray requires a lock-free T");
			using Block = std::array<std::atomic<T>, BLOCK_SIZE>;
		public:
			explicit AtomicSlotArray(uint64_t size) {
				if (size == 0) {
					size = 1;
				}

				Resize(size);
			}

			AtomicSlotArray() = default;

			AtomicSlotArray(const AtomicSlotArray&) = delete;
			AtomicSlotArray& operator=(const AtomicSlotArray&) = delete;
			AtomicSlotArray(AtomicSlotArray&&) = delete;
			AtomicSlotArray& operator=(AtomicSlotArray&&) = delete;

			~AtomicSlotArray() {
				for (uint64_t i = 0; i < BLOCK_COUNT; i++) {
					if (m_Blocks[i] != nullptr) {
						delete m_Blocks[i].load(std::memory_order_acquire);
					}
				}
			}

			[[nodiscard]] uint64_t Size() const {
				return m_Size.load(std::memory_order_acquire);
			}

			void Resize(const uint64_t newSize) {
				const uint64_t oldSize = m_Size.load(std::memory_order_acquire);
				if (newSize != 0 && newSize > oldSize) {
					const uint64_t currentBlockCount = oldSize / BLOCK_SIZE;
					const uint64_t newBlockCount = std::ceil(static_cast<double>(newSize) / static_cast<double>(BLOCK_SIZE));
					CORI_CORE_ASSERT(newBlockCount <= BLOCK_COUNT, "AtomicSlotArray out of memory, trying to resize to '{}' when max block count is '{}'", newBlockCount, BLOCK_COUNT);

					for (uint64_t i = currentBlockCount; i < newBlockCount; i++) {
						Block* alloc = new Block{};
						m_Blocks[i].store(alloc, std::memory_order_release);
					}

					m_Size.store(newBlockCount * BLOCK_SIZE, std::memory_order_release);
				}
			}

			[[nodiscard]] std::atomic<T>& operator[](const uint64_t i) {
				return m_Blocks[i / BLOCK_SIZE].load(std::memory_order_acquire)[i % BLOCK_SIZE];
			}

			[[nodiscard]] std::atomic<T>& At(const uint64_t i) {
				CORI_ASSERT(i < m_Size.load(std::memory_order_acquire), "AtomicSlotArray index out of bounds");
				return m_Blocks[i / BLOCK_SIZE].load(std::memory_order_acquire)[i % BLOCK_SIZE];
			}
		private:
			std::array<std::atomic<Block*>, BLOCK_COUNT> m_Blocks{ nullptr };
			std::atomic<uint64_t> m_Size{ 0 };
		};

		//only used in spokes that live outside main thread
		template<typename T, uint64_t BLOCK_COUNT, uint64_t BLOCK_SIZE>
		class AssetHandleAllocator {
		public:
			//lockfree methods
			[[nodiscard]] bool IsHandleValid(const ConstHandle<T> handle) const {

			}

			void AddRef(const Handle<T> handle) {

			}

			[[nodiscard]] bool TryAddRef(const Handle<T> handle) {

			}

			uint32_t RemoveRef(const Handle<T> handle) {

			}

			[[nodiscard]] AssetID BoundAssetID(const uint32_t index) {

			}

			//under AM mutex only
			[[nodiscard]] Handle<T> Allocate() {

			}

			void Free(const Handle<T> handle) {

			}

			void BindAssetID(const uint32_t index, const AssetID id) {

			}

			void BindDeletionPolicy(const uint32_t index, const AssetDeletionPolicy policy) {

			}

			[[nodiscard]] uint32_t BumpGeneration(const uint32_t index) {

			}

			[[nodiscard]] uint32_t GetGeneration(const uint32_t index) {

			}

		private:
			AtomicSlotArray<uint32_t, BLOCK_COUNT, BLOCK_SIZE> m_Versions;
			AtomicSlotArray<uint32_t, BLOCK_COUNT, BLOCK_SIZE> m_RefCounts;
			AtomicSlotArray<AssetDeletionPolicy, BLOCK_COUNT, BLOCK_SIZE> m_DeletionPolicies;
			AtomicSlotArray<uint64_t, BLOCK_COUNT, BLOCK_SIZE> m_AssetID;

			std::vector<uint32_t> m_LoadGenerations;
			std::deque<uint32_t> m_Holes;
			uint32_t m_ReusedIndexCounter{ 0 };
		};
	}
}