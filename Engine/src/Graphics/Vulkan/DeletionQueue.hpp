#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanImage.hpp"

namespace Cori {
	namespace Graphics {
		class DeletionQueue {
			struct QueuedVirtualAlloc {
				vma::VirtualAllocation alloc;
				vma::VirtualBlock block;
				std::optional<std::weak_ptr<std::mutex>> mutex;
			};

		public:
			~DeletionQueue() {
				FlushAll();
			}

			static void Init();
			static void Shutdown();
			static DeletionQueue& Get();

			static void PushDeleter(std::function<void()>&& deleter) {
				PushDeleter(std::move(deleter), s_DefaultDelay);
			}

			static void PushDeleter(std::function<void()>&& deleter, uint32_t delay);

			static void PushBuffer(VulkanBuffer& buffer) {
				PushBuffer(buffer, s_DefaultDelay);
			}

			static void PushBuffer(VulkanBuffer& buffer, uint32_t delay);

			static void PushImage(VulkanImage& image) {
				PushImage(image, s_DefaultDelay);
			}

			static void PushImage(VulkanImage& image, uint32_t delay);

			static void PushShaderObject(vk::ShaderEXT object) {
				PushShaderObject(object, s_DefaultDelay);
			}

			static void PushShaderObject(vk::ShaderEXT object, uint32_t delay);

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block, const std::optional<std::weak_ptr<std::mutex>>& mutex = std::nullopt) {
				PushVirtualAlloc(allocation, block, s_DefaultDelay, mutex);
			}

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block, uint32_t delay, const std::optional<std::weak_ptr<std::mutex>>& mutex = std::nullopt);

			static void PushVirtualBlock(vma::VirtualBlock block) {
				PushVirtualBlock(block, s_DefaultDelay);
			}

			static void PushVirtualBlock(vma::VirtualBlock block, uint32_t delay);

			static void PushImGuiTexture(const ImTextureID id) {
				PushImGuiTexture(id, s_DefaultDelay);
			}

			static void PushImGuiTexture(const ImTextureID id, uint32_t delay);

			static void Flush();

			static constexpr uint32_t GetMaxDelay() {
				return s_BucketCount - 1;
			}

		protected:
			friend VulkanEngine;

			void FlushAll();

		private:
			DeletionQueue();

			static constexpr uint32_t s_DefaultDelay{ FRAMES_IN_FLIGHT - 1 };
			static constexpr uint32_t s_BucketCount{ FRAMES_IN_FLIGHT * 2 + 2 };
			uint32_t m_Counter{ 0 };

			std::array<std::vector<std::function<void()>>, s_BucketCount> m_Deleters;
			std::array<std::vector<VulkanBuffer>, s_BucketCount> m_BufferQueue;
			std::array<std::vector<VulkanImage>, s_BucketCount> m_ImageQueue;
			std::array<std::vector<vk::ShaderEXT>, s_BucketCount> m_ShaderObjectQueue;
			std::array<std::vector<QueuedVirtualAlloc>, s_BucketCount> m_VirtAllocQueue;
			std::array<std::vector<vma::VirtualBlock>, s_BucketCount> m_VirtBlockQueue;
			std::array<std::vector<ImTextureID>, s_BucketCount> m_ImGuiTextureQueue;

			static std::unique_ptr<DeletionQueue> s_Instance;
		};
	}
}
