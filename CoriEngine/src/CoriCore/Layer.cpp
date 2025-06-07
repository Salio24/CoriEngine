// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Layer.hpp"
#include "ECS/SceneManager.hpp"

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
		std::shared_ptr<Scene> scene = SceneManager::Get().GetScene(name);
		
		bool success = scene->OnBind();

		if (CORI_CORE_ASSERT_ERROR(success, "Failed to bind scene '{0}'!", name)) { return; }
		
		ActiveScene = scene;

		CORI_CORE_TRACE("Layer: Scene '{0}' bound successfully", name);
	}

	void Layer::UnbindScene() {
		if (CORI_CORE_ASSERT_WARN(ActiveScene != nullptr, "No active scene to deactivate!")) { return; }

		CORI_CORE_DEBUG("Layer: Unbinding scene '{0}'", ActiveScene->m_Name);
		bool success = ActiveScene->OnUnbind();
		if (CORI_CORE_ASSERT_ERROR(success, "Failed to unbind scene '{0}'!", ActiveScene->m_Name)) { return; }

		CORI_CORE_TRACE("Layer: Scene '{0}' unbound successfully", ActiveScene->m_Name);
		ActiveScene = nullptr;
	}

}