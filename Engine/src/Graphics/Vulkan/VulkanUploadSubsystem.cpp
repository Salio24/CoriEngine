#include "VulkanUploadSubsystem.hpp"


namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanVirtualBufferAllocator> VulkanVirtualBufferAllocator::s_Instance{ nullptr };

		void VulkanVirtualBufferAllocator::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanVirtualBufferAllocator is already initialized.");
			s_Instance = std::unique_ptr<VulkanVirtualBufferAllocator>(new VulkanVirtualBufferAllocator());
		}

		void VulkanVirtualBufferAllocator::Shutdown() {
			s_Instance.reset();
		}

		VulkanVirtualBufferAllocator& VulkanVirtualBufferAllocator::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanVirtualBufferAllocator::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		std::unique_ptr<VulkanDynamicContainerUploadManager> VulkanDynamicContainerUploadManager::s_Instance{ nullptr };

		void VulkanDynamicContainerUploadManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanDynamicContainerUploadManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanDynamicContainerUploadManager>(new VulkanDynamicContainerUploadManager());
		}

		void VulkanDynamicContainerUploadManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanDynamicContainerUploadManager& VulkanDynamicContainerUploadManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanDynamicContainerUploadManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}