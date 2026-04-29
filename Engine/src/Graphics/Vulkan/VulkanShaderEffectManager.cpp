#include "VulkanShaderEffectManager.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanShaderEffectManager> VulkanShaderEffectManager::s_Instance{ nullptr };

		void VulkanShaderEffectManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderEffectManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanShaderEffectManager>(new VulkanShaderEffectManager());
		}

		void VulkanShaderEffectManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanShaderEffectManager& VulkanShaderEffectManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanShaderEffectManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}