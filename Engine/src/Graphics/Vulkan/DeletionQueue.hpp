#pragma once
#include <deque>
#include <functional>

namespace Cori {
	namespace Graphics {
		class DeletionQueue {
		public:
			void PushDeleter(std::function<void()>&& deleter);

			void Flush();

		private:
			std::deque<std::function<void()>> m_Deleters;
		};
	}
}
