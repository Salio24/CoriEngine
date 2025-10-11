#include "SceneManager.hpp"
#include "Graphics/CameraController.hpp"
#include "Core/Application.hpp"
#include "Systems/System.hpp"
#include "Systems/Trigger.hpp"
#include "Systems/Animation.hpp"
#include "Systems/StateMachine.hpp"
#include "Systems/Hierarchy.hpp"
#include "Systems/Transform.hpp"
#include "Systems/Physics.hpp"

namespace Cori {
	namespace World {
		SceneManager::Data* SceneManager::s_Data{ nullptr };

		struct SceneManager::Data {
			struct TransparentHash {
				using is_transparent = void;
				size_t operator()(std::string_view sv) const noexcept {
					return std::hash<std::string_view>{}(sv);
				}
			};

			struct TransparentEqual {
				using is_transparent = void;
				bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
					return lhs == rhs;
				}
			};

			std::atomic<uint32_t> s_NextSceneID{ 1 };
			std::unordered_map<uint32_t, SceneHandle> m_Handles;
			std::unordered_map<std::string, std::shared_ptr<Scene>, TransparentHash, TransparentEqual> m_Scenes;
		};

		std::expected<SceneHandle, Core::CoriError<>> SceneManager::GetScene(const std::string& name) {
			if (!s_Data->m_Scenes.contains(name)) {
				return std::unexpected(Core::CoriError(std::format("No Scene with name '{}' exists.", name)));
			}

			return SceneHandle(s_Data->m_Scenes.at(name));
		}

		std::expected<SceneHandle, Core::CoriError<>> SceneManager::GetScene(const std::string_view name) {
			if (!s_Data->m_Scenes.contains(name)) {
				return std::unexpected(Core::CoriError(std::format("No Scene with name '{}' exists.", name)));
			}

			return SceneHandle(s_Data->m_Scenes.find(name)->second);
		}

		std::expected<SceneHandle, Core::CoriError<>> SceneManager::GetHandle(const uint32_t sceneID) {
			if (s_Data->m_Handles.contains(sceneID)) {
				return s_Data->m_Handles.at(sceneID);
			}

			return std::unexpected(Core::CoriError(std::format("No scene with ID '{}' found.", sceneID)));
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
			const uint32_t id = s_Data->s_NextSceneID.fetch_add(1, std::memory_order_relaxed);
			scene->m_SceneID = id;
			SceneHandle handle = SceneHandle(scene);
			s_Data->m_Handles.insert({ id, handle });
			scene->RegisterSystem<Systems::Transform>();
			scene->RegisterSystem<Systems::Animation>();
			scene->RegisterSystem<Systems::StateMachine>();
			scene->RegisterSystem<Systems::Hierarchy>();
			return handle;
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
				s_Data->m_Handles.erase(s_Data->m_Scenes.at(name)->m_SceneID);
				s_Data->m_Scenes.erase(name);
				return {};
			}

			return std::unexpected(Core::CoriError(std::format("Failed to destroy Scene '{}', this scene is active in some layer. (ref count is > 1", name)));
		}
	}
}