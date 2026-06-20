#pragma once
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/Threading/ConcurrentHandleAllocator.hpp"

namespace Cori {
	namespace Core {
		//only used in spokes that live outside main thread
		template<typename T, typename UnloadF, uint16_t REUSE_THRESHOLD = 64> requires std::invocable<UnloadF, uint32_t, AssetID>
		class AssetHandleAllocator : public Cori::Threading::ConcurrentHandleAllocatorBase<AssetHandleAllocator<T, UnloadF, REUSE_THRESHOLD>, T, REUSE_THRESHOLD> {
		public:
			void AddRef(const Handle<T> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::AddRef called with an invalid handle");

				m_RefCounts[handle.GetIndex()].fetch_add(1, std::memory_order_relaxed);
			}

			[[nodiscard]] bool TryAddRef(const Handle<T> handle) {
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

			void RemoveRef(const Handle<T> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::BoundAssetID called with an invalid handle");

				uint32_t prev = m_RefCounts[handle.GetIndex()].load(std::memory_order_acq_rel);
				if (prev != 1) {
					return;
				}

				if (m_DeletionPolicies[handle.GetIndex()].load(std::memory_order_acquire) != AssetDeletionPolicy::eRefCounted) {
					return;
				}

				AssetID id = m_AssetIDs[handle.GetIndex()].load(std::memory_order_acquire);
				UnloadF(handle, id);
				//RenderThreadCommandQueue::Push([handle, id]{ T::Manager::Unload(handle, id); });
			}

			[[nodiscard]] AssetID BoundAssetID(const Handle<T> handle) const {
				CORI_CORE_ASSERT(IsHandleValid(handle), "AssetHandleAllocator::BoundAssetID called with an invalid handle");

				return m_AssetIDs[handle.GetIndex()].load(std::memory_order_acquire);
			}

			//under AM mutex only
			void BindAssetID(const uint32_t index, const AssetID id) {
				CORI_CORE_ASSERT(!AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::BindAssetID called with AssetManager mutex unlocked");
				// it is atomic so mutex is not required memory wise, but this method should only be used from asset manager methods that use the lock
				m_AssetIDs[index].store(id, std::memory_order_release);
			}

			void BindDeletionPolicy(const uint32_t index, const AssetDeletionPolicy policy) {
				CORI_CORE_ASSERT(!AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::BindDeletionPolicy called with AssetManager mutex unlocked");
				// it is atomic so mutex is not required memory wise, but this method should only be used from asset manager methods that use the lock
				m_DeletionPolicies[index].store(policy, std::memory_order_release);
			}

			[[nodiscard]] uint32_t BumpGeneration(const uint32_t index) {
				CORI_CORE_ASSERT(!AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::BumpGeneration called with AssetManager mutex unlocked");
				if (index >= m_LoadGenerations.size()) {
					m_LoadGenerations.resize(index * 1.5f);
				}

				return m_LoadGenerations[index]++;
			}

			[[nodiscard]] uint32_t GetGeneration(const uint32_t index) const {
				CORI_CORE_ASSERT(!AssetManager2::GetMutex().try_lock(), "AssetHandleAllocator::GetGeneration called with AssetManager mutex unlocked");
				return m_LoadGenerations[index];
			}

		private:
			void ResizeExtras(const uint64_t newSize) {
				const uint64_t newSizePowerOfTwo = Utility::GetNextPowerOfTwo(newSize);

				if (newSizePowerOfTwo >= m_RefCounts.size()) {
					m_RefCounts.grow_to_at_least(newSizePowerOfTwo);
				}

				if (newSizePowerOfTwo >= m_DeletionPolicies.size()) {
					m_DeletionPolicies.grow_to_at_least(newSizePowerOfTwo);
				}

				if (newSizePowerOfTwo >= m_AssetIDs.size()) {
					m_AssetIDs.grow_to_at_least(newSizePowerOfTwo);
				}
			}

			tbb::concurrent_vector<std::atomic<uint32_t>> m_RefCounts;
			tbb::concurrent_vector<std::atomic<AssetDeletionPolicy>> m_DeletionPolicies;
			tbb::concurrent_vector<std::atomic<uint64_t>> m_AssetIDs;

			std::vector<uint32_t> m_LoadGenerations;
		};
	}
}