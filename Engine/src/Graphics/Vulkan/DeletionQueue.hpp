#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanImage.hpp"

namespace Cori {
	namespace Graphics {
		class DeletionQueue {
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

			static void PushDeleter(std::function<void()>&& deleter, uint32_t delay) {
				if (delay >= s_BucketCount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushDeleter is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
					delay = GetMaxDelay();
				}

				delay = (delay + Get().m_Counter) % s_BucketCount;

				Get().m_Deleters[delay].push_back(std::move(deleter));
			}

			static void PushBuffer(VulkanBuffer& buffer) {
				PushBuffer(buffer, s_DefaultDelay);
			}

			static void PushBuffer(VulkanBuffer& buffer, uint32_t delay) {
				if (delay >= s_BucketCount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushBuffer is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
					delay = GetMaxDelay();
				}

				delay = (delay + Get().m_Counter) % s_BucketCount;

				Get().m_BufferQueue[delay].push_back(buffer);
			}

			static void PushImage(VulkanImage& image) {
				PushImage(image, s_DefaultDelay);
			}

			static void PushImage(VulkanImage& image, uint32_t delay) {
				if (delay >= s_BucketCount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushImage is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
					delay = GetMaxDelay();
				}

				delay = (delay + Get().m_Counter) % s_BucketCount;

				Get().m_ImageQueue[delay].push_back(image);
			}

			static void PushShaderObject(vk::ShaderEXT object) {
				PushShaderObject(object, s_DefaultDelay);
			}

			static void PushShaderObject(vk::ShaderEXT object, uint32_t delay) {
				if (delay >= s_BucketCount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushShaderObject is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
					delay = GetMaxDelay();
				}

				delay = (delay + Get().m_Counter) % s_BucketCount;

				Get().m_ShaderObjectQueue[delay].push_back(object);
			}

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block) {
				PushVirtualAlloc(allocation, block, s_DefaultDelay);
			}

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block, uint32_t delay) {
				if (delay >= s_BucketCount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushVirtualAlloc is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
					delay = GetMaxDelay();
				}

				delay = (delay + Get().m_Counter) % s_BucketCount;

				Get().m_VirtAllocQueue[delay].emplace_back(allocation, block);
			}

			static void PushVirtualBlock(vma::VirtualBlock block) {
				PushVirtualBlock(block, s_DefaultDelay);
			}

			static void PushVirtualBlock(vma::VirtualBlock block, uint32_t delay) {
				if (delay >= s_BucketCount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeletionQueue }, "Delay '{}' provided to PushVirtualBlock is higher or equal than the total bucket count '{}', it will be clamped.", delay, s_BucketCount);
					delay = GetMaxDelay();
				}

				delay = (delay + Get().m_Counter) % s_BucketCount;

				Get().m_VirtBlockQueue[delay].emplace_back(block);
			}

			static void Flush();

			static constexpr uint32_t GetMaxDelay() {
				return s_BucketCount - 1;
			}

		protected:
			friend VulkanEngine;

			void FlushAll();

		private:
			DeletionQueue() {
				for (uint32_t i = 0; i < s_BucketCount; i++) {
					m_Deleters[i].reserve(64);
					m_BufferQueue[i].reserve(64);
					m_ImageQueue[i].reserve(64);
					m_VirtAllocQueue[i].reserve(64);
					m_VirtBlockQueue[i].reserve(64);
				}
			}

			static constexpr uint32_t s_DefaultDelay{ FRAMES_IN_FLIGHT - 1 };
			static constexpr uint32_t s_BucketCount{ FRAMES_IN_FLIGHT * 2 + 2 };
			uint32_t m_Counter{ 0 };

			std::array<std::vector<std::function<void()>>, s_BucketCount> m_Deleters;
			std::array<std::vector<VulkanBuffer>, s_BucketCount> m_BufferQueue;
			std::array<std::vector<VulkanImage>, s_BucketCount> m_ImageQueue;
			std::array<std::vector<vk::ShaderEXT>, s_BucketCount> m_ShaderObjectQueue;
			std::array<std::vector<std::pair<vma::VirtualAllocation, vma::VirtualBlock>>, s_BucketCount> m_VirtAllocQueue;
			std::array<std::vector<vma::VirtualBlock>, s_BucketCount> m_VirtBlockQueue;


			static std::unique_ptr<DeletionQueue> s_Instance;
		};
	}
}
