#pragma once

#if 1
namespace Cori {

		//FIXME: remove Threading namespace
		namespace Threading {
			class MainThreadQueue {
			public:
				template <class F, class... Args>
				std::future<std::invoke_result_t<F, Args...>> Submit(F&& f, Args&&... args) {
					using ReturnType = std::invoke_result_t<F, Args...>;

					auto task = std::make_shared<std::packaged_task<ReturnType()>>(
						std::bind(std::forward<F>(f), std::forward<Args>(args)...)
					);

					std::future<ReturnType> res = task->get_future();

					{
						std::unique_lock lock(m_Mutex);

						m_Tasks.emplace_back([task]() {
							(*task)();
						});
					}

					return res;
				}

				void Execute() {
					CORI_PROFILE_FUNCTION();
					std::deque<std::function<void()>> copy;
					{
						std::lock_guard lock(m_Mutex);
						copy.swap(m_Tasks);
					}

					for (const auto& task : copy) {
						task();
					}
				}
			private:
				std::deque<std::function<void()>> m_Tasks;
				std::mutex m_Mutex;
			};
		}

}
#endif