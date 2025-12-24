#include "DeletionQueue.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<DeletionQueue> DeletionQueue::s_Instance{ nullptr };

		void DeletionQueue::Init() {
			CORI_CORE_ASSERT(!s_Instance, "DeletionQueue is already initialized.");
			s_Instance = std::unique_ptr<DeletionQueue>(new DeletionQueue());
		}

		void DeletionQueue::Shutdown() {
			s_Instance.reset();
		}

		DeletionQueue& DeletionQueue::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling DeletionQueue::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void DeletionQueue::Flush() {
			uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();

			for (auto& deleter : Get().m_Deleters[frameIndex] | std::views::reverse) {
				deleter();
			}

			Get().m_Deleters[frameIndex].clear();

			for (auto& buffer : Get().m_BufferQueue[frameIndex] | std::views::reverse) {
				buffer.Destroy();
			}

			Get().m_BufferQueue[frameIndex].clear();

			for (auto& image : Get().m_ImageQueue[frameIndex] | std::views::reverse) {
				image.Destroy();
			}

			Get().m_ImageQueue[frameIndex].clear();

			for (auto [alloc, block] : Get().m_VirtAllocQueue[frameIndex] | std::views::reverse) {
				block.free(alloc);
			}

			Get().m_VirtAllocQueue[frameIndex].clear();
		}

		void DeletionQueue::FlushAll() {
			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
				for (auto& deleter : Get().m_Deleters[i] | std::views::reverse) {
					deleter();
				}

				Get().m_Deleters[i].clear();

				for (auto& buffer : Get().m_BufferQueue[i] | std::views::reverse) {
					buffer.Destroy();
				}

				Get().m_BufferQueue[i].clear();

				for (auto& image : Get().m_ImageQueue[i] | std::views::reverse) {
					image.Destroy();
				}

				Get().m_ImageQueue[i].clear();

				for (auto [alloc, block] : Get().m_VirtAllocQueue[i] | std::views::reverse) {
					block.free(alloc);
				}

				Get().m_VirtAllocQueue[i].clear();
			}
		}
	}
}