#include "DeletionQueue.hpp"

namespace Cori {
	namespace Graphics {
		void DeletionQueue::PushDeleter(std::function<void()>&& deleter) {
			m_Deleters.push_back(deleter);
		}

		void DeletionQueue::Flush() {
			for (auto it = m_Deleters.rbegin(); it != m_Deleters.rend(); ++it) {
				(*it)();
			}
		}

	}
}