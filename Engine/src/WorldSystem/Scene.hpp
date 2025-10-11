#pragma once
#include "Entity.hpp"
#include "Profiling/Trackable.hpp"
#include "Graphics/CameraController.hpp"
#include "Physics/Physics.hpp"
#include "EntityView.hpp"
#include "Core/Time.hpp"
#include "Systems/Concept.hpp"
#include <ska_sort.hpp>

namespace Cori {
	namespace Core {
		class Layer;
	}
	namespace World {
		/**
		 * @brief A scene in a game world.
		 * @details You can have several scenes loaded at the same time, but you can only have one scene bound per layer. Use SceneHandle to interact with a scene, don't try to use Scene directly.
		 */
		class Scene : public Profiling::Trackable<Scene> {
		public:
			~Scene();

			Entity CreateBlankEntity();
			Entity CreateEntity(const std::string& name, const Utility::HashedTag64& tag);
			void DestroyEntity(Entity entity);

			std::expected<void, Core::CoriError<>> AddEntityToCache(const Entity entity, const Utility::StringHash32 key);
			[[nodiscard]] std::expected<Entity, Core::CoriError<>> GetEntityFromCache(const Utility::StringHash32 key);
			void RemoveEntityFromCache(const Utility::StringHash32 key);

			[[nodiscard]] std::expected<Entity, Core::CoriError<>> FindEntity(const std::string& name);
			[[nodiscard]] std::expected<Entity, Core::CoriError<>> FindEntity(const std::string& name, const Utility::HashedTag64& tag);
			[[nodiscard]] std::vector<Entity> GetEntitiesWithTag(const Utility::HashedTag64& tag);

			template<typename... T>
			[[nodiscard]] auto View() {
				auto view = m_Registry.view<T...>();
				return EntityView(view, m_Registry);
			}

			template<typename... T, typename... ExcludeT>
			[[nodiscard]] auto View(Exclude<ExcludeT...>) {
				auto view = m_Registry.view<T...>(entt::exclude<ExcludeT...>);
				return EntityView(view, m_Registry);
			}

			// untested and unused for now
			//template<typename... T, typename Func>
			//void ForEach(Func func) {
			//	m_Registry.view<T...>().each(func);
			//}

			template<typename T, typename... Args>
			T& AddContextComponent(Args&&... args) {
				return m_Registry.ctx().emplace<T>(std::forward<Args>(args)...);
			}

			template<typename T>
			[[nodiscard]] T& GetContextComponent() {
				if (!HasContextComponent<T>()) {
					CORI_CORE_FATAL_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Failed to get context component '{}' from scene '{}'.", CORI_CLEAN_TYPE_NAME(T), m_Name);
				}
				return m_Registry.ctx().get<T>();
			}

			template<typename T>
			[[nodiscard]] const T& GetContextComponent() const {
				if (!HasContextComponent<T>()) {
					CORI_CORE_FATAL_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Failed to get const context component '{}' from scene '{}'.", CORI_CLEAN_TYPE_NAME(T), m_Name);
				}
				return m_Registry.ctx().get<const T>();
			}

			template<typename T>
			[[nodiscard]] bool HasContextComponent() const {
				return m_Registry.ctx().contains<T>();
			}

			template<typename T>
			void RemoveContextComponent() {
				m_Registry.ctx().erase<T>();
			}

			template <typename T, typename... Args> requires IsSystem<T>
			void RegisterSystem(Args&&... args) {
				if (m_RegisteredSystems.contains(std::type_index(typeid(T)))) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Trying to register '{}' twice for scene '{}', you can't register a system twice.", CORI_CLEAN_TYPE_NAME(T), m_Name);
					return;
				}

				std::shared_ptr<T> system = std::make_shared<T>();
				system->SetOwnerScene(this);
				const bool success = system->Create(std::forward<Args>(args)...);
				if (success) {
					auto systemType = std::type_index(typeid(T));
					m_SystemPriority.emplace_back(T::Priority, systemType);
					ska_sort(
						m_SystemPriority.begin(),
						m_SystemPriority.end(),
						[](const std::pair<SystemPriority, std::type_index>& entry) -> SystemPriority {
							return entry.first;
						});

					m_RegisteredSystems.insert({ systemType, std::move(system) });
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "System '{}' has been registered for scene '{}'", CORI_CLEAN_TYPE_NAME(T), m_Name);
					return;
				}

				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Failed to register System '{}' with scene '{}', Create returned false.", CORI_CLEAN_TYPE_NAME(T), m_Name);
			}

			template <typename T> requires IsSystem<T>
			void UnregisterSystem() {
				if (m_RegisteredSystems.contains(std::type_index(typeid(T)))) {
					m_RegisteredSystems.erase(std::type_index(typeid(T)));
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "System '{}' has been unregistered for scene '{}'", CORI_CLEAN_TYPE_NAME(T), m_Name);
				}
			}

			template <typename T> requires IsSystem<T>
			std::expected<std::weak_ptr<T>, Core::CoriError<>> GetSystem() {
				if (m_RegisteredSystems.contains(std::type_index(typeid(T)))) {
					return std::weak_ptr<T>(std::static_pointer_cast<T>(m_RegisteredSystems[std::type_index(typeid(T))]));
				}

				return std::unexpected(Core::CoriError("Failed to get system, system is not registered."));
			}

			[[nodiscard]] Graphics::CameraController& GetCameraController() {
				return m_ActiveCamera;
			}

			[[nodiscard]] const  Graphics::CameraController& GetCameraController() const {
				return m_ActiveCamera;
			}

			[[nodiscard]] std::string_view GetName() const {
				return m_Name;
			}

			[[nodiscard]] uint32_t GetSceneID() const {
				return m_SceneID;
			}

			void BeginRender();

			void EndRender();

		protected:
			//friend Core::Layer;
			friend class SceneHandle;
			friend class SceneManager;
			[[nodiscard]] bool OnBind();
			[[nodiscard]] bool OnUnbind();

			void OnUpdate(Core::GameTimer& gameTimer);

			void OnTickUpdate(Core::GameTimer& gameTimer);

			void OnImGuiRender(Core::GameTimer& gameTimer);

			[[nodiscard]] static std::shared_ptr<Scene> Create(std::string name);

			entt::registry m_Registry;
		private:
			//friend class Entity;
			explicit Scene(std::string name);

			uint32_t m_SceneID;

			Graphics::CameraController m_ActiveCamera;

			std::string m_Name;

			std::unordered_map<std::type_index, std::shared_ptr<System>> m_RegisteredSystems;
			std::vector<std::pair<SystemPriority, std::type_index>> m_SystemPriority;

			std::unordered_map<Core::UUID, entt::entity> m_UUIDToEntity;
			std::unordered_map<Utility::StringHash32, entt::entity> m_EntityCache;

		};
	}
}
