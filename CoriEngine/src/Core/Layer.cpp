#include "Layer.hpp"
#include "SceneSystem/SceneManager.hpp"


namespace Cori {
	
	Layer::Layer(const std::string& name) : m_DebugName(name) {
		CORI_CORE_INFO("Layer {0} created", m_DebugName);
	}

	Layer::~Layer() {
		CORI_CORE_INFO("Layer {0} destroyed", m_DebugName);
	}

	void Layer::OnAttach() {
		CORI_CORE_INFO("Layer {0} attached", m_DebugName);
	}

	void Layer::OnDetach() {
		CORI_CORE_INFO("Layer {0} detached", m_DebugName);
	}

	void Layer::BindScene(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(!name.empty(), "Scene name cannot be empty!")) { return; }

		CORI_CORE_DEBUG("Layer: Binding scene '{0}'", name);
		std::shared_ptr<Scene> scene = SceneManager::GetScene(name);
		
		bool success = scene->OnBind();
		if (CORI_CORE_ASSERT_ERROR(success, "Failed to bind scene '{0}'!", name)) { return; }
		
		ActiveScene.m_SceneRaw = scene;
		CORI_CORE_TRACE("Layer: Scene '{0}' bound successfully", name);
	}

	void Layer::UnbindScene() {
		if (CORI_CORE_ASSERT_WARN(ActiveScene.IsValid(), "No active scene to deactivate!")) { return; }

		CORI_CORE_DEBUG("Layer: Unbinding scene '{0}'", ActiveScene.GetName());

		bool success = ActiveScene.OnUnbind();
		if (CORI_CORE_ASSERT_ERROR(success, "Failed to unbind scene '{0}'!", GetName())) { return; }

		CORI_CORE_TRACE("Layer: Scene '{0}' unbound successfully", ActiveScene.GetName());
		ActiveScene.m_SceneRaw = nullptr;
	}
}