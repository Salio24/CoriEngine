#include "SceneManager.hpp"
#include "Renderer/CameraController.hpp"
#include "Core/Application.hpp"

namespace Cori {
	SceneManager::Data* SceneManager::s_Data{nullptr};

	struct SceneManager::Data {
		std::unordered_map<std::string, std::shared_ptr<Scene>> m_Scenes;
	};

	std::shared_ptr<Scene> SceneManager::GetScene(const std::string& name) {
		CORI_CORE_ASSERT_FATAL(s_Data->m_Scenes.contains(name), "No scene with name '{}' exists", name);
		return s_Data->m_Scenes.at(name);
	}

	void SceneManager::Init() {
		s_Data = new Data();
	}

	void SceneManager::Shutdown() {
		delete s_Data;
	}

	std::shared_ptr<Scene> SceneManager::CreateScene(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "Scene name cannot be empty!")) { return nullptr; }
		if (CORI_CORE_VERIFY_ERROR(!s_Data->m_Scenes.contains(name), "Scene '{0}' already exists!", name)) { return nullptr; }

		CORI_CORE_INFO("SceneManager: Creating scene '{0}'", name);

		std::shared_ptr<Scene> scene = Scene::Create(name);
		s_Data->m_Scenes.insert({ name, scene });
		return scene;
	}

	void SceneManager::DestroyScene(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "Scene name cannot be empty!")) { return; }
		if (CORI_CORE_ASSERT_ERROR(s_Data->m_Scenes.contains(name), "Can't destroy scene, scene '{0}' does not exist!", name)) { return; }

		CORI_CORE_INFO("SceneManager: Destroying scene '{0}'", name);

		if (s_Data->m_Scenes.at(name).use_count() == 1) {
			s_Data->m_Scenes.erase(name);
		}
		else {
			CORI_CORE_WARN("SceneManager: Failed to destroy scene '{0}', this scene is active in some layer. (ref count is > 1)", name);
		}
	}

}