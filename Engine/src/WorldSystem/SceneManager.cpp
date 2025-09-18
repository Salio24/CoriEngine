#include "SceneManager.hpp"
#include "Graphics/CameraController.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace World {
		SceneManager::Data* SceneManager::s_Data{ nullptr };

		struct SceneManager::Data {
			std::unordered_map<std::string, std::shared_ptr<Scene>> m_Scenes;
		};

		std::expected<SceneHandle, Core::CoriError<>> SceneManager::GetScene(const std::string& name) {
			if (!s_Data->m_Scenes.contains(name)) {
				return std::unexpected(Core::CoriError(std::format("No Scene with name '{}' exists.", name)));
			}

			return SceneHandle(s_Data->m_Scenes.at(name));
		}

		void SceneManager::Init() {
			s_Data = new Data();
		}

		void SceneManager::Shutdown() {
			delete s_Data;
		}

		std::expected<SceneHandle, Core::CoriError<>> SceneManager::CreateScene(const std::string& name) {
			if (name.empty()) {
				return std::unexpected(Core::CoriError("Scene name cannot be empty!"));
			}

			if (s_Data->m_Scenes.contains(name)) {
				return std::unexpected(Core::CoriError(std::format("Scene with name '{}' already exists.", name)));
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::SceneManager }, "Creating Scene '{}'", name);

			std::shared_ptr<Scene> scene = Scene::Create(name);
			s_Data->m_Scenes.insert({ name, scene });
			return SceneHandle(scene);
		}

		std::expected<void, Core::CoriError<>> SceneManager::DestroyScene(const std::string& name) {
			if (name.empty()) {
				return std::unexpected(Core::CoriError("Scene name cannot be empty!"));
			}

			if (!s_Data->m_Scenes.contains(name)) {
				return std::unexpected(Core::CoriError(std::format("No Scene with name '{}' exists.", name)));
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::SceneManager }, "Destroying Scene '{}'", name);

			if (s_Data->m_Scenes.at(name).use_count() == 1) {
				s_Data->m_Scenes.erase(name);
				return {};
			}

			return std::unexpected(Core::CoriError(std::format("Failed to destroy Scene '{}', this scene is active in some layer. (ref count is > 1", name)));
		}
	}
}