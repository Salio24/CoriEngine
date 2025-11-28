#include "VulkanShaderManager.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanShaderManager> VulkanShaderManager::s_Instance{ nullptr };

		void VulkanShaderManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanShaderManager>(new VulkanShaderManager());
		}

		void VulkanShaderManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanShaderManager& VulkanShaderManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanShaderManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}