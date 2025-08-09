#pragma once
#include "Entity.hpp"
#include "Core/SelfFactory.hpp"
#include "Profiling/Trackable.hpp"
#include "Renderer/CameraController.hpp"
#include "EventSystem/Event.hpp"
#include "Physics/Physics.hpp"
#include "EntityView.hpp"

namespace Cori {
	class Scene : public Profiling::Trackable<Scene>, public SharedSelfFactory<Scene> {
	public:
		static bool PreCreateHook([[maybe_unused]] const std::string& name) { return true; }
		explicit Scene(const std::string& name);
		~Scene();

		bool OnBind();
		bool OnUnbind();

		void OnUpdate(const double deltaTime);

		void OnTickUpdate(const float timeStep);

		Entity CreateEntity(const std::string& name, const Utility::HashedTag64& tag);
		void DestroyEntity(Entity entity);

		std::expected<void, const char*> AddEntityToCache(Entity entity, const Utility::StringHash32 tag);
		std::expected<Entity, const char*> GetEntityFromCache(const Utility::StringHash32 tag);
		void RemoveEntityFromCache(const Utility::StringHash32 key);

		std::expected<Entity, const char*> FindEntity(const std::string& name);
		std::expected<Entity, const char*> FindEntity(const std::string& name, const Utility::HashedTag64& tag);

		template<typename... Component>
		auto View() {
			auto view = m_Registry.view<Component...>();
			return EntityView(view, m_Registry);
		}

		template<typename... T, typename... ExcludeT>
		auto View(Exclude<ExcludeT...>) {
			auto view = m_Registry.view<T...>(entt::exclude<ExcludeT...>);
			return EntityView(view, m_Registry);
		}

		template<typename... T, typename Func>
		void ForEach(Func func) {
			m_Registry.view<T...>().each(func);
		}

		template<typename T, typename... Args>
		T& AddContextComponent(Args&&... args) {
			return m_Registry.ctx().emplace<T>(std::forward<Args>(args)...);
		}

		template<typename T, typename... Args>
		T& AddOrAssignContextComponent(Args&&... args) {
			return m_Registry.ctx().insert_or_assign<T>(std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetContextComponent() {
			return m_Registry.ctx().get<T>();
		}

		template<typename T>
		const T& GetContextComponent() const {
			return m_Registry.ctx().get<const T>();
		}

		template<typename T>
		bool HasContextComponent() const {
			return m_Registry.ctx().contains<T>();
		}

		template<typename T>
		void RemoveContextComponent() {
			m_Registry.ctx().erase<T>();
		}

		CameraController m_ActiveCamera;

		Physics::PhysicsWorld m_PhysicsWorld;

		std::string m_Name;

		//friend struct Components::Entity::StateMachine;
	private:
		entt::registry m_Registry;

		void UpdateTransform();
		void UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, uint8_t parentLayer, bool parentTransformDirty, bool parentLayerDirty);

		void OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity);

		std::unordered_map<Core::UUID, entt::entity> m_UUIDToEntity;
		std::unordered_map<Utility::StringHash32, entt::entity> m_EntityCache;

		friend class Entity;
	};

}
