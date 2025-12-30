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
				uint32_t frame = GetDstFrame();
				Get().m_Deleters[frame].push_back(std::move(deleter));
			}

			static void PushDeleter(std::function<void()>&& deleter, const uint32_t dstFrame) {
				Get().m_Deleters[dstFrame].push_back(std::move(deleter));
			}

			static void PushBuffer(VulkanBuffer& buffer) {
				uint32_t frame = GetDstFrame();
				Get().m_BufferQueue[frame].push_back(buffer);
			}

			static void PushBuffer(VulkanBuffer& buffer, uint32_t dstFrame) {
				Get().m_BufferQueue[dstFrame].push_back(buffer);
			}

			static void PushImage(VulkanImage& image) {
				uint32_t frame = GetDstFrame();
				Get().m_ImageQueue[frame].push_back(image);
			}

			static void PushImage(VulkanImage& image, const uint32_t dstFrame) {
				Get().m_ImageQueue[dstFrame].push_back(image);
			}

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block) {
				uint32_t frame = GetDstFrame();
				Get().m_VirtAllocQueue[frame].emplace_back(allocation, block);
			}

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block, const uint32_t dstFrame) {
				Get().m_VirtAllocQueue[dstFrame].emplace_back(allocation, block);
			}

			static void PushVirtualBlock(vma::VirtualBlock block) {
				uint32_t frame = GetDstFrame();
				Get().m_VirtBlockQueue[frame].emplace_back(block);
			}

			static void PushVirtualBlock(vma::VirtualBlock block, const uint32_t dstFrame) {
				Get().m_VirtBlockQueue[dstFrame].emplace_back(block);
			}

			static void Flush();

		protected:
			friend VulkanEngine;

			void FlushAll();

		private:
			DeletionQueue() {
				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					m_Deleters[i].reserve(64);
					m_BufferQueue[i].reserve(64);
					m_ImageQueue[i].reserve(64);
					m_VirtAllocQueue[i].reserve(64);
					m_VirtBlockQueue[i].reserve(64);
				}
			}
			
			[[nodiscard]] static uint32_t GetDstFrame() {
				return VulkanEngine::GetPreviousFrameInFlight();
			}

			std::array<std::vector<std::function<void()>>, FRAMES_IN_FLIGHT> m_Deleters;
			std::array<std::vector<VulkanBuffer>, FRAMES_IN_FLIGHT> m_BufferQueue;
			std::array<std::vector<VulkanImage>, FRAMES_IN_FLIGHT> m_ImageQueue;
			std::array<std::vector<std::pair<vma::VirtualAllocation, vma::VirtualBlock>>, FRAMES_IN_FLIGHT> m_VirtAllocQueue;
			std::array<std::vector<vma::VirtualBlock>, FRAMES_IN_FLIGHT> m_VirtBlockQueue;

			static std::unique_ptr<DeletionQueue> s_Instance;
		};
	}
}
