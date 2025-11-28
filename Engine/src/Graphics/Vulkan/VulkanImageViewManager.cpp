#include "VulkanImageViewManager.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanImageViewManager> VulkanImageViewManager::s_Instance{ nullptr };

		void VulkanImageViewManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanImageViewManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanImageViewManager>(new VulkanImageViewManager());
		}

		void VulkanImageViewManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanImageViewManager& VulkanImageViewManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanImageViewManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}