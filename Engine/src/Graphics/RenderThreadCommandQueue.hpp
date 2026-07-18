#pragma once
#include "RenderThreadWakeup.hpp"

namespace Cori {
	namespace Graphics {
		class RenderThreadCommandQueue {
			using Command = std::move_only_function<void()>;
		public:
			template <typename Fn>
			static void Push(Fn&& fn) {
				if (IsInlineMode()) {
					std::forward<Fn>(fn)();
					return;
				}

				{
					std::lock_guard lk(s_Mutex);
					s_Queue.emplace_back(std::forward<Fn>(fn));
					s_PushCount.fetch_add(1, std::memory_order_release);
				}

				RenderThreadWakeup::Wake();
			}

			static uint64_t CurrentPushCount() {
				return s_PushCount.load(std::memory_order_acquire);
			}

			static uint64_t DrainOnRenderThread() {
				{
					std::lock_guard lk(s_Mutex);
					std::swap(s_Queue, s_Draining);
				}

				for (auto& cmd : s_Draining) {
					cmd();
				}

				s_DrainedCount.fetch_add(s_Draining.size(), std::memory_order_release);
				const uint64_t n = s_Draining.size();
				s_Draining.clear();
				return n;
			}

			static uint64_t DrainedCount() {
				return s_DrainedCount.load(std::memory_order_acquire);
			}

			static void SetExecuterThreadId(const std::thread::id id) {
				s_ExecutorThreadId.store(id, std::memory_order_release);
			}

			static void ClearExecuterThreadId() {
				s_ExecutorThreadId.store({}, std::memory_order_release);
			}

			static bool IsInlineMode() {
				return std::this_thread::get_id() == s_ExecutorThreadId.load(std::memory_order_acquire);
			}
			
		private:
			static inline std::mutex s_Mutex;
			static inline std::vector<Command> s_Queue;
			static inline std::vector<Command> s_Draining;
			static inline std::atomic<uint64_t> s_PushCount{ 0 };
			static inline std::atomic<uint64_t> s_DrainedCount{ 0 };
			static inline std::atomic<std::thread::id> s_ExecutorThreadId{};
		};
	}
}
