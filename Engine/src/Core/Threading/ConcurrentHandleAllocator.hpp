#pragma once
#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/concurrent_vector.h>
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "Utility/BitHelpers.hpp"

namespace Cori {
	namespace Threading {
		template<typename Derived, typename T, uint16_t REUSE_THRESHOLD>
		class ConcurrentHandleAllocatorBase {
		public:
			[[nodiscard]] Core::Handle<T> Allocate() {
				uint32_t index;
				uint32_t version;
				bool popped = false;

				uint32_t currentCounter = m_ReusedIndexCounter.load(std::memory_order_acquire);
				while (currentCounter > 0) {
					if (m_ReusedIndexCounter.compare_exchange_weak(currentCounter, currentCounter - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
						if (m_Holes.try_pop(index)) {
							popped = true;
						}
						else {
							m_ReusedIndexCounter.store(0, std::memory_order_release);
						}

						break;
					}
				}

				if (!popped && m_Holes.unsafe_size() > REUSE_THRESHOLD) {
					uint32_t expected = 0;
					const uint32_t desired = std::max<std::ptrdiff_t>(0, m_Holes.unsafe_size());

					if (desired > REUSE_THRESHOLD && m_ReusedIndexCounter.compare_exchange_strong(expected, desired - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
						if (m_Holes.try_pop(index)) {
							popped = true;
						} else {
							m_ReusedIndexCounter.store(0, std::memory_order_release);
						}
					}
				}

				if (popped) {
					version = m_Versions[index].load(std::memory_order_acquire);
				}
				else {
					index = m_NextIndex.fetch_add(1, std::memory_order_relaxed);
					Resize(index + 1);
					m_Versions[index].store(1, std::memory_order_release);
					version = 1;
				}

				auto handle = Core::Handle<T>{ index, version };
				static_cast<Derived*>(this)->AllocateExtras(handle);
				return handle;
			}

			void Free(const Core::Handle<T> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "ConcurrentHandleAllocatorBase::Free called with an invalid handle")

				uint32_t index = handle.GetIndex();
				m_Versions[index].fetch_add(1, std::memory_order_release);
				static_cast<Derived*>(this)->FreeExtras(handle);
				m_Holes.push(index);
			}

			[[nodiscard]] bool IsHandleValid(const Core::ConstHandle<T> handle) const {
				return handle.GetIndex() < m_Versions.size() && m_Versions[handle.GetIndex()].load(std::memory_order_acquire) == handle.GetVersion() && handle.GetVersion() != 0;
			}

			void Resize(const uint64_t newSize) {
				const uint64_t newSizePowerOfTwo = Utility::GetNextPowerOfTwo(newSize);

				if (newSizePowerOfTwo >= m_Versions.size()) {
					m_Versions.grow_to_at_least(newSizePowerOfTwo);
				}

				static_cast<Derived*>(this)->ResizeExtras(newSize);
			}

		private:
			tbb::concurrent_vector<std::atomic<uint32_t>> m_Versions;
			tbb::concurrent_queue<uint32_t> m_Holes;
			std::atomic<uint32_t> m_ReusedIndexCounter{ 0 };
			std::atomic<uint32_t> m_NextIndex{ 0 };
		};

		template<typename T, uint16_t REUSE_THRESHOLD>
		class ConcurrentHandleAllocator : public ConcurrentHandleAllocatorBase<ConcurrentHandleAllocator<T, REUSE_THRESHOLD>, T, REUSE_THRESHOLD> {
		public:
			void ResizeExtras([[maybe_unused]] const uint64_t newSize) {}
			void AllocateExtras([[maybe_unused]] const Core::Handle<T> handle) {}
			void FreeExtras([[maybe_unused]] const Core::Handle<T> handle) {}
		};
	}
}