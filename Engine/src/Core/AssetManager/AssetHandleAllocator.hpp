#pragma once
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/Threading/ConcurrentHandleAllocator.hpp"

namespace Cori {
	namespace Core {
		//only used in spokes that live outside main thread
		//TODO: add the asset concept here
		template<typename T, uint16_t REUSE_THRESHOLD = 64>
		class AssetHandleAllocator : public Threading::ConcurrentHandleAllocatorBase<AssetHandleAllocator<T, REUSE_THRESHOLD>, T, REUSE_THRESHOLD> {
		public:
			void AddRef(const Handle<T> handle) {
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::AddRef called with an invalid handle");

				m_RefCounts[handle.GetIndex()].fetch_add(1, std::memory_order_relaxed);
			}

			[[nodiscard]] bool TryAddRef(const Handle<T> handle) {
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::TryAddRef called with an invalid handle");

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
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::RemoveRef called with an invalid handle");

				uint32_t prev = m_RefCounts[handle.GetIndex()].fetch_sub(1, std::memory_order_release);
				if (prev != 1) {
					return;
				}
				std::atomic_thread_fence(std::memory_order_acquire);

				//const uint32_t n = m_ReservedCount.load(std::memory_order_acquire);
				//for (uint32_t i = 0; i < n; i++) {
				//	if (Handle<T>(m_ReservedHandles[i].load(std::memory_order_relaxed)) == handle) {
				//		return;
				//	}
				//}

				CORI_CORE_ASSERT(AssetManager2::GetDeletionPoliciesVector()[GetBoundVectorKey(handle)].load(std::memory_order_acquire) == AssetDeletionPolicy::eRefCounted, "Keep-alive slot reached terminal zero — self-ref missing.");

				//if (AssetManager2::GetDeletionPoliciesVector()[GetBoundVectorKey(handle)].load(std::memory_order_acquire) != AssetDeletionPolicy::eRefCounted) {
				//	return;
				//}

				T::Manager::QueueUnload(handle);
			}

			[[nodiscard]] AssetID GetBoundAssetID(const Handle<T> handle) const {
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::GetBoundAssetID called with an invalid handle");
				AssetID result = m_AssetIDs[handle.GetIndex()].load(std::memory_order_acquire);
				CORI_CORE_ASSERT(result != UINT64_MAX, "AssetHandleAllocator::GetBoundAssetID called with a reserved handle.");
				return result;
			}

			[[nodiscard]] uint32_t GetBoundVectorKey(const Handle<T> handle) const {
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::GetBoundVectorKey called with an invalid handle");
				uint32_t vectorKey = m_VectorKeys[handle.GetIndex()].load(std::memory_order_acquire);
				CORI_CORE_ASSERT(vectorKey != UINT32_MAX, "AssetHandleAllocator::GetBoundVectorKey called with a reserved handle.");
				return vectorKey;
			}

			[[nodiscard]] uint32_t GetGeneration(const Handle<T> handle) const {
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::GetGeneration called with an invalid handle");
				return m_LoadGenerations[handle.GetIndex()].load(std::memory_order_acquire);
			}

			void SetAssetStatus(const Handle<T> handle, const AssetStatus newStatus) {
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::SetAssetStatus called with an invalid handle");
				m_AssetStatuses[handle.GetIndex()].store(newStatus, std::memory_order_release);
			}

			[[nodiscard]] AssetStatus GetAssetStatus(const Handle<T> handle) const {
				CORI_CORE_ASSERT(this->IsHandleValid(handle), "AssetHandleAllocator::GetAssetStatus called with an invalid handle");
				return m_AssetStatuses[handle.GetIndex()].load(std::memory_order_acquire);
			}

			void UnbindAsset(const Handle<T> handle) {
				m_VectorKeys[handle.GetIndex()].store(UINT32_MAX, std::memory_order_release);
				m_AssetIDs[handle.GetIndex()].store(UINT64_MAX, std::memory_order_release);
			}

			void BindAsset(const Handle<T> handle, const AssetID id, const uint32_t vectorKey) {
				m_AssetIDs[handle.GetIndex()].store(id, std::memory_order_release);
				m_VectorKeys[handle.GetIndex()].store(vectorKey, std::memory_order_release);
			}

			[[nodiscard]] uint32_t BumpGeneration(const Handle<T> handle) {
				return m_LoadGenerations[handle.GetIndex()].fetch_add(1, std::memory_order_relaxed) + 1;
			}

		private:
			friend class Threading::ConcurrentHandleAllocatorBase<AssetHandleAllocator<T, REUSE_THRESHOLD>, T, REUSE_THRESHOLD>;

			void ResizeExtras(const uint64_t newSize) {
				const uint64_t newSizePowerOfTwo = Utility::GetNextPowerOfTwo(newSize);

				if (newSizePowerOfTwo >= m_RefCounts.size()) {
					m_RefCounts.grow_to_at_least(newSizePowerOfTwo);
				}

				if (newSizePowerOfTwo >= m_VectorKeys.size()) {
					m_VectorKeys.grow_to_at_least(newSizePowerOfTwo);
				}

				if (newSizePowerOfTwo >= m_AssetIDs.size()) {
					m_AssetIDs.grow_to_at_least(newSizePowerOfTwo);
				}

				if (newSizePowerOfTwo >= m_LoadGenerations.size()) {
					m_LoadGenerations.grow_to_at_least(newSizePowerOfTwo);
				}

				if (newSizePowerOfTwo >= m_AssetStatuses.size()) {
					m_AssetStatuses.grow_to_at_least(newSizePowerOfTwo);
				}
			}

			void AllocateExtras(const Handle<T> handle) {
				UnbindAsset(handle);
				m_RefCounts[handle.GetIndex()].store(1, std::memory_order_release);
				m_AssetStatuses[handle.GetIndex()].store(AssetStatus::eUnloaded, std::memory_order_release);
				if constexpr (requires { T::Manager::AllocateExtras(handle); }) {
					T::Manager::AllocateExtras(handle);
				}
			}

			void FreeExtras(const Handle<T> handle) {
				m_LoadGenerations[handle.GetIndex()].store(0, std::memory_order_release);
				if constexpr (requires { T::Manager::FreeExtras(handle); }) {
					T::Manager::FreeExtras(handle);
				}
			}

			tbb::concurrent_vector<std::atomic<uint32_t>> m_RefCounts;
			tbb::concurrent_vector<std::atomic<uint32_t>> m_VectorKeys;
			tbb::concurrent_vector<std::atomic<uint64_t>> m_AssetIDs;
			tbb::concurrent_vector<std::atomic<uint32_t>> m_LoadGenerations;
			tbb::concurrent_vector<std::atomic<AssetStatus>> m_AssetStatuses;

			std::array<std::atomic<uint64_t>, 8> m_ReservedHandles{};
			std::atomic<uint32_t> m_ReservedCount{ 0 };
		};
	}
}