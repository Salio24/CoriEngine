#pragma once
#include "Core/AssetManager/AssetManager2.hpp"
#include "RenderThreadCommandQueue.hpp"

namespace Cori {
	namespace Graphics {
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
				#ifdef DEBUG_BUILD
				return At(i);
				#else
				return m_Blocks[i / BLOCK_SIZE].load(std::memory_order_acquire)[i % BLOCK_SIZE];
				#endif
			}

			[[nodiscard]] std::atomic<T>& At(const uint64_t i) {
				CORI_ASSERT(i < m_Size.load(std::memory_order_acquire), "AtomicSlotArray index '{}' out of bounds", i);
				return m_Blocks[i / BLOCK_SIZE].load(std::memory_order_acquire)[i % BLOCK_SIZE];
			}
		private:
			std::array<std::atomic<Block*>, BLOCK_COUNT> m_Blocks{ nullptr };
			std::atomic<uint64_t> m_Size{ 0 };
		};

		//only used in spokes that live outside main thread
		template<typename T, uint16_t REUSE_THRESHOLD = 64, uint64_t BLOCK_COUNT = 1024, uint64_t BLOCK_SIZE = 1024>
		class AssetHandleAllocator {
		public:
			//lockfree methods
			[[nodiscard]] bool IsHandleValid(const Core::ConstHandle<T> handle) const {
				return handle.GetIndex() < m_Versions.size() && m_Versions[handle.GetIndex()].load(std::memory_order_acquire) == handle.GetVersion();
			}

			void AddRef(const Core::Handle<T> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::AddRef called with an invalid handle");

				m_RefCounts[handle.GetIndex()].fetch_add(1, std::memory_order_relaxed);
			}

			[[nodiscard]] bool TryAddRef(const Core::Handle<T> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::TryAddRef called with an invalid handle");

				auto& rc = m_RefCounts[handle.GetIndex()];
				uint32_t cur = rc.load(std::memory_order_relaxed);

				while (cur != 0) {
					if (rc.compare_exchange_weak(cur, cur + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
						return true;
					}
				}

				return false;
			}

			void RemoveRef(const Core::Handle<T> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::BoundAssetID called with an invalid handle");

				uint32_t prev = m_RefCounts[handle.GetIndex()].load(std::memory_order_acq_rel);
				if (prev != 1) {
					return;
				}

				if (m_DeletionPolicies[handle.GetIndex()].load(std::memory_order_acquire) != Core::AssetDeletionPolicy::eRefCounted) {
					return;
				}

				Core::AssetID id = m_AssetIDs[handle.GetIndex()].load(std::memory_order_acquire);
				RenderThreadCommandQueue::Push([handle, id]{ T::Manager::Unload(handle, id); });
			}

			[[nodiscard]] Core::AssetID BoundAssetID(const Core::Handle<T> handle) const {
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::BoundAssetID called with an invalid handle");

				return m_AssetIDs[handle.GetIndex()].load(std::memory_order_acquire);
			}

			//under AM mutex only
			[[nodiscard]] Core::Handle<T> Allocate() {
				CORI_CORE_ASSERT(!Core::AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::Allocate called with AssetManager mutex unlocked");

				uint32_t index;
				uint32_t version;

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

					version = m_Versions[index].load(std::memory_order_acquire);
				} else {
					index = m_NextIndex++;
					Resize(m_NextIndex);
					m_Versions[index].store(1, std::memory_order_release);
					version = 1;
				}

				return Core::Handle<T>{ index, version };
			}

			void Free(const Core::Handle<T> handle) {
				CORI_CORE_ASSERT(!Core::AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::Free called with AssetManager mutex unlocked");
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::Free called with an invalid handle")

				m_Versions[handle.GetIndex()].fetch_add(1, std::memory_order_release);
				m_Holes.emplace_back(handle.GetIndex());
			}

			void BindAssetID(const uint32_t index, const Core::AssetID id) {
				CORI_CORE_ASSERT(!Core::AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::BindAssetID called with AssetManager mutex unlocked");
				// it is atomic so mutex is not required memory wise, but this method should only be used from asset manager methods that use the lock
				m_AssetIDs[index].store(id, std::memory_order_release);
			}

			void BindDeletionPolicy(const uint32_t index, const Core::AssetDeletionPolicy policy) {
				CORI_CORE_ASSERT(!Core::AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::BindDeletionPolicy called with AssetManager mutex unlocked");
				// it is atomic so mutex is not required memory wise, but this method should only be used from asset manager methods that use the lock
				m_DeletionPolicies[index].store(policy, std::memory_order_release);
			}

			[[nodiscard]] uint32_t BumpGeneration(const uint32_t index) {
				CORI_CORE_ASSERT(!Core::AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::BumpGeneration called with AssetManager mutex unlocked");
				return m_LoadGenerations[index]++;
			}

			[[nodiscard]] uint32_t GetGeneration(const uint32_t index) const {
				CORI_CORE_ASSERT(!Core::AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::GetGeneration called with AssetManager mutex unlocked");
				return m_LoadGenerations[index];
			}

			void Resize(const uint64_t newSize) {
				m_Versions.Resize(newSize);
				m_RefCounts.Resize(newSize);
				m_DeletionPolicies.Resize(newSize);
				m_AssetIDs.Resize(newSize);

				if (newSize >= m_LoadGenerations.size()) {
					m_LoadGenerations.resize(newSize * 1.5f);
				}
			}

		private:
			AtomicSlotArray<uint32_t, BLOCK_COUNT, BLOCK_SIZE> m_Versions;
			AtomicSlotArray<uint32_t, BLOCK_COUNT, BLOCK_SIZE> m_RefCounts;
			AtomicSlotArray<Core::AssetDeletionPolicy, BLOCK_COUNT, BLOCK_SIZE> m_DeletionPolicies;
			AtomicSlotArray<uint64_t, BLOCK_COUNT, BLOCK_SIZE> m_AssetIDs;

			std::vector<uint32_t> m_LoadGenerations;
			std::deque<uint32_t> m_Holes;
			uint32_t m_ReusedIndexCounter{ 0 };
			uint32_t m_NextIndex{ 0 };
		};
	}
}