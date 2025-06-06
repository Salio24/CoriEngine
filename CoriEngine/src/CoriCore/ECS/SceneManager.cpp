#include "SceneManager.hpp"
#include "../Renderer/CameraController.hpp"

namespace Cori {
	Scene* SceneManager::CreateSceneImpl(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "SceneManager::CreateScene: Scene name cannot be empty!")) { return nullptr; }
		if (CORI_CORE_ASSERT_ERROR(!m_Scenes.contains(name), "SceneManager::CreateScene: Scene '{0}' already exists!", name)) { return nullptr; }

		CORI_CORE_INFO("SceneManager: Creating scene '{0}'", name);

		std::unique_ptr<Scene> scene = Scene::Create(name);
		m_Scenes.insert({ name, std::move(scene) });
		return m_Scenes.at(name).get();
	}

	void SceneManager::DestroySceneImpl(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "SceneManager::DestroyScene: Scene name cannot be empty!")) { return; }
		if (CORI_CORE_ASSERT_ERROR(m_Scenes.contains(name), "SceneManager::DestroyScene: Scene '{0}' does not exist!", name)) { return; }

		CORI_CORE_INFO("SceneManager: Destroying scene '{0}'", name);
		UnbindSceneImpl();
		m_Scenes.erase(name);
	}

	void SceneManager::BindSceneImpl(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "SceneManager::BindScene: Scene name cannot be empty!")) { return; }
		if (CORI_CORE_ASSERT_ERROR(m_Scenes.contains(name), "SceneManager::BindScene: Scene '{0}' does not exist!", name)) { return; }

		CORI_CORE_DEBUG("SceneManager: Binding scene '{0}'", name);
		Scene* scene = m_Scenes.at(name).get();
		bool success = scene->OnBind();
		if (CORI_CORE_ASSERT_ERROR(success, "SceneManager::BindScene: Failed to bind scene '{0}'!", name)) { return; }
		CameraController::BindCameraComponent(&scene->GetContextComponent<CameraContextComponent>());
		CORI_CORE_TRACE("SceneManager: Scene '{0}' bound successfully", name);
		m_ActiveScene = m_Scenes.at(name).get();
	}

	void SceneManager::UnbindSceneImpl() {
		if (CORI_CORE_ASSERT_WARN(m_ActiveScene != nullptr, "SceneManager::UnbindScene: No active scene to deactivate!")) { return; }

		CORI_CORE_DEBUG("SceneManager: Unbinding scene '{0}'", m_ActiveScene->m_Name);
		Scene* scene = m_Scenes.at(m_ActiveScene->m_Name).get();
		bool success = scene->OnUnbind();
		if (CORI_CORE_ASSERT_ERROR(success, "SceneManager::UnbindScene: Failed to unbind scene '{0}'!", m_ActiveScene->m_Name)) { return; }
		CameraController::UnbindCameraComponent();
		CORI_CORE_TRACE("SceneManager: Scene '{0}' unbound successfully", m_ActiveScene->m_Name);
		m_ActiveScene = nullptr;
	}

}