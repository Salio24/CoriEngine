#pragma once

namespace Cori {
	namespace Threading {
		class OneAtATime {
		public:
			explicit OneAtATime(std::atomic<bool>& storage) : m_Storage(storage) {}

			bool TryLock() {
				bool expected = false;
				if (m_Storage.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
					m_Own = true;
					return true;
				}

				return false;
			}

			~OneAtATime() {
				if (m_Own) {
					m_Storage.store(false, std::memory_order_release);
				}
			}

		private:
			std::atomic<bool>& m_Storage;
			bool m_Own{ false };
		};
	}
}
