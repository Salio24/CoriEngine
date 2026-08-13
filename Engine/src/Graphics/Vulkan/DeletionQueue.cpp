#include "DeletionQueue.hpp"
#include "VmaLeakLog.hpp"
#include "backends/imgui_impl_vulkan.h"

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

			for (const auto& [alloc, block, mutex] : Get().m_VirtAllocQueue[Get().m_Counter] | std::views::reverse) {
				if (mutex) {
					auto locked = mutex->lock();
					if (locked) {
						std::lock_guard lk(*locked);
						block.free(alloc);
						continue;
					}
				}

				block.free(alloc);
			}

			Get().m_VirtAllocQueue[Get().m_Counter].clear();

			for (auto shaderObject : Get().m_ShaderObjectQueue[Get().m_Counter] | std::views::reverse) {
				VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObject);
			}

			Get().m_ShaderObjectQueue[Get().m_Counter].clear();

			for (const auto id : Get().m_ImGuiTextureQueue[Get().m_Counter]) {
				ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(id));
			}

			Get().m_ImGuiTextureQueue[Get().m_Counter].clear();

			for (auto& block : Get().m_VirtBlockQueue[Get().m_Counter] | std::views::reverse) {
				if (!block.isVirtualBlockEmpty()) {
					const vma::Statistics statistics = block.getVirtualBlockStatistics();
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Deleting vma virtual block, but {} allocation(s) holding {} bytes made from this block are still not freed, they will be forcibly freed now. You should generally free all allocation before deleting a block, not doing that can lead to a crash under certain circumstances (e.g., freeing a virtual allocation after the block was deleted via deletion queue in the next frame).", statistics.allocationCount, static_cast<uint64_t>(statistics.allocationBytes));
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

				for (auto [alloc, block, mutex] : m_VirtAllocQueue[i] | std::views::reverse) {
					if (mutex) {
						auto locked = mutex->lock();
						if (locked) {
							std::lock_guard lk(*locked);
							block.free(alloc);
							continue;
						}
					}

					block.free(alloc);
				}

				m_VirtAllocQueue[i].clear();

				for (const auto id : m_ImGuiTextureQueue[i]) {
					ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(id));
				}

				m_ImGuiTextureQueue[i].clear();
			}

			for (uint32_t i = 0; i < s_BucketCount; i++) {
				for (auto& block : m_VirtBlockQueue[i] | std::views::reverse) {
					if (!block.isVirtualBlockEmpty()) {
						const vma::Statistics statistics = block.getVirtualBlockStatistics();
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Flushing the deletion queue with {} allocation(s) (this is always 0 for block with linear algorithm) holding {} bytes still live in a vma virtual block, they will be forcibly freed now.", statistics.allocationCount, static_cast<uint64_t>(statistics.allocationBytes));
					}

					block.clearVirtualBlock();

					block.clearVirtualBlock();
					block.destroy();
				}

				m_VirtBlockQueue[i].clear();
			}
		}
		void DeletionQueue::PushDeleter(std::function<void()>&& deleter, uint32_t delay) {
			if (delay >= s_BucketCount) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushDeleter is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
				delay = GetMaxDelay();
			}

			delay = (delay + Get().m_Counter) % s_BucketCount;

			Get().m_Deleters[delay].push_back(std::move(deleter));
		}

		void DeletionQueue::PushBuffer(VulkanBuffer& buffer, uint32_t delay) {
			if (delay >= s_BucketCount) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushBuffer is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
				delay = GetMaxDelay();
			}

			delay = (delay + Get().m_Counter) % s_BucketCount;

			Get().m_BufferQueue[delay].push_back(buffer);
		}

		void DeletionQueue::PushImage(VulkanImage& image, uint32_t delay) {
			if (delay >= s_BucketCount) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushImage is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
				delay = GetMaxDelay();
			}

			delay = (delay + Get().m_Counter) % s_BucketCount;

			Get().m_ImageQueue[delay].push_back(image);
		}

		void DeletionQueue::PushShaderObject(vk::ShaderEXT object, uint32_t delay) {
			if (delay >= s_BucketCount) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushShaderObject is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
				delay = GetMaxDelay();
			}

			delay = (delay + Get().m_Counter) % s_BucketCount;

			Get().m_ShaderObjectQueue[delay].push_back(object);
		}

		void DeletionQueue::PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block, uint32_t delay, const std::optional<std::weak_ptr<std::mutex>>& mutex) {
			if (delay >= s_BucketCount) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushVirtualAlloc is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
				delay = GetMaxDelay();
			}

			delay = (delay + Get().m_Counter) % s_BucketCount;

			Get().m_VirtAllocQueue[delay].emplace_back(allocation, block, mutex);
		}

		void DeletionQueue::PushVirtualBlock(vma::VirtualBlock block, uint32_t delay) {
			if (delay >= s_BucketCount) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushVirtualBlock is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
				delay = GetMaxDelay();
			}

			delay = (delay + Get().m_Counter) % s_BucketCount;

			Get().m_VirtBlockQueue[delay].emplace_back(block);
		}

		void DeletionQueue::PushImGuiTexture(const ImTextureID id, uint32_t delay) {
			if (delay >= s_BucketCount) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushImGuiTexture is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
				delay = GetMaxDelay();
			}

			delay = (delay + Get().m_Counter) % s_BucketCount;

			Get().m_ImGuiTextureQueue[delay].emplace_back(id);
		}

		DeletionQueue::DeletionQueue() {
			for (uint32_t i = 0; i < s_BucketCount; i++) {
				m_Deleters[i].reserve(64);
				m_BufferQueue[i].reserve(64);
				m_ImageQueue[i].reserve(64);
				m_VirtAllocQueue[i].reserve(64);
				m_VirtBlockQueue[i].reserve(64);
			}
		}

	}
}
