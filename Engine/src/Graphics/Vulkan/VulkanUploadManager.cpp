#include "VulkanUploadManager.hpp"
#if 0

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanUploadManager> VulkanUploadManager::s_Instance{ nullptr };

		void VulkanUploadManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanUploadManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanUploadManager>(new VulkanUploadManager());
		}

		void VulkanUploadManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanUploadManager& VulkanUploadManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanUploadManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}

#endif