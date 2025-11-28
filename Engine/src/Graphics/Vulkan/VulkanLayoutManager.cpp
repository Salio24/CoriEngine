#include "VulkanLayoutManager.hpp"
namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanGlobalLayoutManager> VulkanGlobalLayoutManager::s_Instance{ nullptr };

		void VulkanGlobalLayoutManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanGlobalLayoutManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanGlobalLayoutManager>(new VulkanGlobalLayoutManager());
		}

		void VulkanGlobalLayoutManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanGlobalLayoutManager& VulkanGlobalLayoutManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanGlobalLayoutManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}