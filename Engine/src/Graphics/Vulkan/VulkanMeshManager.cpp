#include "VulkanMeshManager.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanMeshManager> VulkanMeshManager::s_Instance{ nullptr };

		void VulkanMeshManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderManager is already initialized.")
			s_Instance = std::unique_ptr<VulkanMeshManager>(new VulkanMeshManager());
		}

		void VulkanMeshManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanMeshManager& VulkanMeshManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanMeshManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}