#include "SceneManager.hpp"
#include "Renderer/CameraController.hpp"

namespace Cori {
	std::shared_ptr<Scene> SceneManager::CreateSceneImpl(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "Scene name cannot be empty!")) { return nullptr; }
		if (CORI_CORE_ASSERT_ERROR(!m_Scenes.contains(name), "Scene '{0}' already exists!", name)) { return nullptr; }

		CORI_CORE_INFO("SceneManager: Creating scene '{0}'", name);

		std::shared_ptr<Scene> scene = Scene::Create(name);
		m_Scenes.insert({ name, scene });
		return scene;
	}

	void SceneManager::DestroySceneImpl(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "Scene name cannot be empty!")) { return; }
		if (CORI_CORE_ASSERT_ERROR(m_Scenes.contains(name), "Can't destroy scene, scene '{0}' does not exist!", name)) { return; }

		CORI_CORE_INFO("SceneManager: Destroying scene '{0}'", name);

		if (m_Scenes.at(name).use_count() == 1) {
			m_Scenes.erase(name);
		}
		else {
			CORI_CORE_WARN("SceneManager: Failed to destroy scene '{0}', this scene is active in some layer. (ref count is > 1)", name);
		}
	}

}