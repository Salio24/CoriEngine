#pragma once
//implementation based on https://github.com/rigtorp/SPSCQueue

namespace Cori {
	namespace Threading {
		template<typename T>
		class SPSCRing {
		public:
			using ValueType = T;
			
			explicit SPSCRing(const uint64_t capacity) : m_Capacity(capacity) {
				if (m_Capacity == 0) {
					m_Capacity = 1;
				}

				m_Capacity++;

				m_Capacity = std::clamp(m_Capacity, 2ul, UINT64_MAX - 2 * s_SlotPadding);

				m_Storage = static_cast<T*>(::operator new((m_Capacity + 2 * s_SlotPadding) * sizeof(T), std::align_val_t{ alignof(T) }));
			}

			~SPSCRing() {
				while (Front()) {
					Pop();
				}

				::operator delete(m_Storage, std::align_val_t{ alignof(T) });
			}

			SPSCRing(const SPSCRing&) = delete;
			SPSCRing& operator=(const SPSCRing&) = delete;
			SPSCRing(const SPSCRing&&) = delete;
			SPSCRing& operator=(const SPSCRing&&) = delete;

			template<typename... Args>
			void Emplace(Args&&... args) noexcept {
				const uint64_t head = m_Head.load(std::memory_order_relaxed);
				uint64_t nextHead = head + 1;
				if (nextHead == m_Capacity) {
					nextHead = 0;
				}

				while (nextHead == m_TailCache) {
					nextHead = m_Head.load(std::memory_order_acquire);
				}

				new (&m_Storage[head + s_SlotPadding]) T(std::forward<Args>(args)...);
				m_Head.store(nextHead, std::memory_order_release);
			}

			template<typename... Args>
			[[nodiscard]] bool TryEmplace(Args&&... args) noexcept {
				const uint64_t head = m_Head.load(std::memory_order_relaxed);
				uint64_t nextHead = head + 1;
				if (nextHead == m_Capacity) {
					nextHead = 0;
				}

				if (nextHead == m_TailCache) {
					m_TailCache = m_Tail.load(std::memory_order_acquire);
					if (nextHead == m_TailCache) {
						return false;
					}
				}

				new (&m_Storage[head + s_SlotPadding]) T(std::forward<Args>(args)...);
				m_Head.store(nextHead, std::memory_order_release);
				return true;
			}

			template<typename F, typename... Args>
			void EmplaceSpinWork(F&& work, Args&&... args) noexcept {
				const uint64_t head = m_Head.load(std::memory_order_relaxed);
				uint64_t nextHead = head + 1;
				if (nextHead == m_Capacity) {
					nextHead = 0;
				}

				while (nextHead == m_TailCache) {
					nextHead = m_Head.load(std::memory_order_acquire);
					work();
				}

				new (&m_Storage[head + s_SlotPadding]) T(std::forward<Args>(args)...);
				m_Head.store(nextHead, std::memory_order_release);
			}

			[[nodiscard]] T* Front() noexcept {
				const uint64_t tail = m_Tail.load(std::memory_order_relaxed);
				if (tail == m_HeadCache) {
					m_HeadCache = m_Head.load(std::memory_order_acquire);
					if (m_HeadCache == tail) {
						return nullptr;
					}
				}

				return &m_Storage[tail + s_SlotPadding];
			}

			[[nodiscard]] T* FrontWait() noexcept {
				const uint64_t tail = m_Tail.load(std::memory_order_relaxed);
				while (tail != m_HeadCache) {
					m_HeadCache = m_Head.load(std::memory_order_acquire);
				}

				return &m_Storage[tail + s_SlotPadding];
			}

			void Pop() noexcept {
				const uint64_t tail = m_Tail.load(std::memory_order_relaxed);
				CORI_CORE_ASSERT(m_Head.load(std::memory_order_acquire) != tail, "SPSCRing::Pop, nothing to pop.");

				m_Storage[tail + s_SlotPadding].~T();
				uint64_t nextTail = tail + 1;
				if (nextTail == m_Capacity) {
					nextTail = 0;
				}

				m_Head.store(nextTail, std::memory_order_release);
			}

			[[nodiscard]] uint64_t Size() const noexcept {
				int64_t diff = m_Head.load(std::memory_order_acquire) - m_Tail.load(std::memory_order_acquire);

				if (diff < 0) {
					diff += m_Capacity;
				}

				return diff;
			}

			[[nodiscard]] uint64_t Capacity() const noexcept {
				return m_Capacity - 1;
			}

			[[nodiscard]] bool Empty() const noexcept {
				return m_Head.load(std::memory_order_acquire) == m_Tail.load(std::memory_order_acquire);
			}

		private:
			T* m_Storage;
			uint64_t m_Capacity;

			alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> m_Head{ 0 };
			alignas(std::hardware_destructive_interference_size) uint64_t m_HeadCache{ 0 };
			alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> m_Tail{ 0 };
			alignas(std::hardware_destructive_interference_size) uint64_t m_TailCache{ 0 };

			static constexpr uint64_t s_SlotPadding{ (std::hardware_destructive_interference_size - 1) / sizeof(T) + 1 };
		};
	}
}