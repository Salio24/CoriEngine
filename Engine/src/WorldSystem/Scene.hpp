#pragma once
#include "Entity.hpp"
#include "Profiling/Trackable.hpp"
#include "Graphics/CameraController.hpp"
#include "Physics/Physics.hpp"
#include "EntityView.hpp"

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


			[[nodiscard]] Physics::PhysicsWorld& GetPhysicsWorld() {
				return m_PhysicsWorld;
			}

			[[nodiscard]] const  Physics::PhysicsWorld& GetPhysicsWorld() const {
				return m_PhysicsWorld;
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

		protected:
			//friend Core::Layer;
			friend class SceneHandle;
			friend class SceneManager;
			[[nodiscard]] bool OnBind();
			[[nodiscard]] bool OnUnbind();

			void OnUpdate(const double deltaTime);

			void OnTickUpdate(const float timeStep);


			[[nodiscard]] static std::shared_ptr<Scene> Create(std::string name);
		private:
			//friend class Entity;
			explicit Scene(std::string name);

			entt::registry m_Registry;

			void UpdateTransform();
			void UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, const uint8_t parentDepth, const bool parentTransformDirty, const bool parentDepthDirty);

			void OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity);

			Graphics::CameraController m_ActiveCamera;

			Physics::PhysicsWorld m_PhysicsWorld;

			std::string m_Name;

			std::unordered_map<Core::UUID, entt::entity> m_UUIDToEntity;
			std::unordered_map<Utility::StringHash32, entt::entity> m_EntityCache;

		};
	}
}
