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
			for (auto& deleter : Get().m_Deleters[Get().m_Counter] | std::views::reverse) {
				deleter();
			}

			Get().m_Deleters[Get().m_Counter].clear();

			for (auto& buffer : Get().m_BufferQueue[Get().m_Counter] | std::views::reverse) {
				buffer.Destroy();
			}

			Get().m_BufferQueue[Get().m_Counter].clear();

			for (auto& image : Get().m_ImageQueue[Get().m_Counter] | std::views::reverse) {
				image.Destroy();
			}

			Get().m_ImageQueue[Get().m_Counter].clear();

			for (auto [alloc, block] : Get().m_VirtAllocQueue[Get().m_Counter] | std::views::reverse) {
				block.free(alloc);
			}

			Get().m_VirtAllocQueue[Get().m_Counter].clear();

			for (auto shaderObject : Get().m_ShaderObjectQueue[Get().m_Counter] | std::views::reverse) {
				VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObject);
			}

			Get().m_ShaderObjectQueue[Get().m_Counter].clear();


			for (auto& block : Get().m_VirtBlockQueue[Get().m_Counter] | std::views::reverse) {
				if (!block.isVirtualBlockEmpty()) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Deleting vma virtual block, but some allocations made from this block are still not freed, they will be forcibly freed now. You should generally free all allocation before deleting a block, not doing that can lead to a crash under certain circumstances (e.g., freeing a virtual allocation after the block was deleted via deletion queue in the next frame).");
					block.clearVirtualBlock();
				}

				block.destroy();
			}

			Get().m_VirtBlockQueue[Get().m_Counter].clear();

			Get().m_Counter = (Get().m_Counter + 1) % s_BucketCount;
		}

		void DeletionQueue::FlushAll() {
			for (uint32_t i = 0; i < s_BucketCount; i++) {
				for (auto& deleter : m_Deleters[i] | std::views::reverse) {
					deleter();
				}

				m_Deleters[i].clear();

				for (auto& buffer : m_BufferQueue[i] | std::views::reverse) {
					buffer.Destroy();
				}

				m_BufferQueue[i].clear();

				for (auto& image : m_ImageQueue[i] | std::views::reverse) {
					image.Destroy();
				}

				m_ImageQueue[i].clear();

				for (auto shaderObject : m_ShaderObjectQueue[i] | std::views::reverse) {
					VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObject);
				}

				m_ShaderObjectQueue[i].clear();

				for (auto [alloc, block] : m_VirtAllocQueue[i] | std::views::reverse) {
					block.free(alloc);
				}

				m_VirtAllocQueue[i].clear();
			}

			for (uint32_t i = 0; i < s_BucketCount; i++) {
				for (auto& block : m_VirtBlockQueue[i] | std::views::reverse) {
					block.clearVirtualBlock();
					block.destroy();
				}

				m_VirtBlockQueue[i].clear();
			}
		}
	}
}