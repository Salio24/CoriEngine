#pragma once

namespace Cori {
	namespace Graphics {
		class RenderThreadWakeup {
		public:
			static void Wake() {
				m_Counter.fetch_add(1, std::memory_order_release);
				m_Counter.notify_all();
			}

			[[nodiscard]] static uint64_t Snapshot() {
				return m_Counter.load(std::memory_order_acquire);
			}

			static void WaitChanged(uint64_t prev) {
				m_Counter.wait(prev, std::memory_order_acquire);
			}

		private:
			static inline std::atomic<uint64_t> m_Counter{ 0 };
		};
	}
}