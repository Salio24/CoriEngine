#include "VulkanMaterialSystem.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanMaterialSystem> VulkanMaterialSystem::s_Instance{ nullptr };

		void VulkanMaterialSystem::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanMaterialSystem is already initialized.");
			s_Instance = std::unique_ptr<VulkanMaterialSystem>(new VulkanMaterialSystem());
		}

		void VulkanMaterialSystem::Shutdown() {
			s_Instance.reset();
		}

		VulkanMaterialSystem& VulkanMaterialSystem::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanMaterialSystem::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}