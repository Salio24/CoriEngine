#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanImage.hpp"

namespace Cori {
	namespace Graphics {
		class DeletionQueue {
		public:
			~DeletionQueue() {

			}

			static void Init();
			static void Shutdown();
			static DeletionQueue& Get();

			static void PushDeleter(std::function<void()>&& deleter) {
				uint32_t prevFrame = VulkanEngine::GetPreviousFrameInFlight();
				Get().m_Deleters[prevFrame].push_back(std::move(deleter));
			}

			static void PushDeleter(std::function<void()>&& deleter, const uint32_t dstFrame) {
				Get().m_Deleters[(dstFrame + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT].push_back(std::move(deleter));
			}

			static void PushBuffer(VulkanBuffer& buffer) {
				uint32_t prevFrame = VulkanEngine::GetPreviousFrameInFlight();
				Get().m_BufferQueue[prevFrame].push_back(buffer);
			}

			static void PushBuffer(VulkanBuffer& buffer, uint32_t dstFrame) {
				Get().m_BufferQueue[(dstFrame + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT].push_back(buffer);
			}

			static void PushImage(VulkanImage& image) {
				uint32_t prevFrame = VulkanEngine::GetPreviousFrameInFlight();
				Get().m_ImageQueue[prevFrame].push_back(image);
			}

			static void PushImage(VulkanImage& image, const uint32_t dstFrame) {
				Get().m_ImageQueue[(dstFrame + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT].push_back(image);
			}

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block) {
				uint32_t prevFrame = VulkanEngine::GetPreviousFrameInFlight();
				Get().m_VirtAllocQueue[prevFrame].emplace_back(allocation, block);
			}

			static void PushVirtualAlloc(vma::VirtualAllocation allocation, vma::VirtualBlock block, const uint32_t dstFrame) {
				Get().m_VirtAllocQueue[(dstFrame + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT].emplace_back(allocation, block);
			}

			static void Flush();

		protected:
			friend VulkanEngine;

			static void FlushAll();

		private:
			DeletionQueue() {

			}

			std::array<std::vector<std::function<void()>>, FRAMES_IN_FLIGHT> m_Deleters;
			std::array<std::vector<VulkanBuffer>, FRAMES_IN_FLIGHT> m_BufferQueue;
			std::array<std::vector<VulkanImage>, FRAMES_IN_FLIGHT> m_ImageQueue;
			std::array<std::vector<std::pair<vma::VirtualAllocation, vma::VirtualBlock>>, FRAMES_IN_FLIGHT> m_VirtAllocQueue;

			static std::unique_ptr<DeletionQueue> s_Instance;
		};
	}
}
