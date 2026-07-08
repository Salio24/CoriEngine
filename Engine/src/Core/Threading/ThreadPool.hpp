#pragma once

#if 1
namespace Cori {

		namespace Threading {
			class ThreadPool {
			public:
				explicit ThreadPool(const uint16_t numThreads) : m_WorkerCount(numThreads) {
					for (uint16_t i = 0; i < numThreads; ++i) {
						m_Workers.emplace_back([this] {
							while (true) {
								std::function<void()> task;
								{
									std::unique_lock lock(this->m_QueueMutex);
									this->m_Condition.wait(lock, [this] {
										return this->m_Stop || !this->m_Tasks.empty();
									});

									if (this->m_Stop && this->m_Tasks.empty()) {
										return;
									}

									task = std::move(this->m_Tasks.front());
									this->m_Tasks.pop();
								}
								task();
							}
						});
					}
				}

				~ThreadPool() {
					{
						std::unique_lock lock(m_QueueMutex);
						m_Stop = true;
					}
					m_Condition.notify_all();
					for (std::thread& worker : m_Workers) {
						worker.join();
					}
				}

				/**
				 * @brief Returns a number of threads allocated for this thread pool.
				 * @return Number of threads.
				 */
				[[nodiscard]] uint16_t GetWorkerCount() const {
					return m_WorkerCount;
				}

				/**
				 * @brief Submits a task to be executed on the thread of this thread pool.
				 * @details It is safe to call this function from any thread.
				 * @tparam F Auto deduced callable type.
				 * @tparam Args Auto deduced callable argument types.
				 * @param f Task callable, no specific signature required.
				 * @param args Arguments that will be passed to the callable task upon execution.
				 * @return Future that will hold the result of invoke result of the passed callable.
				 */
				template <class F, class... Args>
				std::future<std::invoke_result_t<F, Args...>> Submit(F&& f, Args&&... args) {
					using ReturnType = std::invoke_result_t<F, Args...>;

					auto task = std::make_shared<std::packaged_task<ReturnType()>>(
						std::bind(std::forward<F>(f), std::forward<Args>(args)...)
					);

					std::future<ReturnType> res = task->get_future();

					{
						std::unique_lock lock(m_QueueMutex);

						CORI_CORE_ASSERT(!m_Stop, "Submit on stopped ThreadPool");

						m_Tasks.emplace([task]() { (*task)(); });
					}
					m_Condition.notify_one();

					return res;
				}

			private:
				std::vector<std::thread> m_Workers;
				std::queue<std::function<void()>> m_Tasks;
				std::mutex m_QueueMutex;
				std::condition_variable m_Condition;
				uint16_t m_WorkerCount{ 0 };
				bool m_Stop{ false };
			};
		}

}
#endif
