#include "ThreadPool.hpp"

#include "GlobalFuncs.hpp"

namespace Cori {
	namespace Threading {
		ThreadPool::ThreadPool(const uint16_t numThreads) : m_WorkerCount(numThreads) {
			for (uint16_t i = 0; i < numThreads; ++i) {
				m_Workers.emplace_back([this, i] {
					SetThreadName("Worker-" + std::to_string(i));
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

		void ThreadPool::Stop() {
			{
				std::unique_lock lock(m_QueueMutex);
				if (m_Stop) {
					return;
				}
				m_Stop = true;
			}
			m_Condition.notify_all();
			for (std::thread& worker : m_Workers) {
				worker.join();
			}
		}

		ThreadPool::~ThreadPool() {
			{
				std::unique_lock lock(m_QueueMutex);
				if (m_Stop) {
					return;
				}
				m_Stop = true;
			}
			m_Condition.notify_all();
			for (std::thread& worker : m_Workers) {
				worker.join();
			}
		}

	}
}
