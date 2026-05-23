#include "VulkanTextureManager.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanTextureManager> VulkanTextureManager::s_Instance{ nullptr };

		void VulkanTextureManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanTextureManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanTextureManager>(new VulkanTextureManager());
		}

		void VulkanTextureManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanTextureManager& VulkanTextureManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanTextureManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}