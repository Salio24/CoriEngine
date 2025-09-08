#include "Layer.hpp"
#include "WorldSystem/SceneManager.hpp"


namespace Cori {
	namespace Core {
		Layer::Layer(std::string name) : m_Name(std::move(name)) {
			if (m_Name.empty()) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Creating a Layer with an empty name. This WILL cause issues.");
			}
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' created.", m_Name);
		}

		Layer::~Layer() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' destroyed.", m_Name);
		}

		void Layer::OnAttach() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' attached.", m_Name);
		}

		void Layer::OnDetach() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' detached.", m_Name);
		}

		std::expected<void, CoriError<>> Layer::BindScene(const std::string& name) {
			if (name.empty()) {
				return std::unexpected(CoriError("Scene name cannot be empty!"));
			}

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Binding Scene '{}' to Layer '{}'", name, m_Name);
			const auto result = World::SceneManager::GetScene(name);
			if (!result) {
				return std::unexpected(result.error());
			}

			const bool success = result.value()->OnBind();
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to bind Scene '{}'", name)));
			}

			ActiveScene.m_SceneRaw = result.value();
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Scene '{}' bound to Layer '{}' successfully", name, m_Name);
			return {};
		}

		std::expected<void, CoriError<>> Layer::UnbindScene() {
			if (ActiveScene.IsValid()) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "No Scene is currently bound to Layer '{}', nothing to unbind.", m_Name);
				return {};
			}

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Unbinding scene '{}' from Layer '{}'", ActiveScene.GetName(), m_Name);

			const bool success = ActiveScene.OnUnbind();
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to unbind Scene '{}'", ActiveScene.GetName())));
			}

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Scene '{}' unbound from Layer '{}' successfully", ActiveScene.GetName(), m_Name);
			ActiveScene.m_SceneRaw = nullptr;
			return {};
		}
	}
}