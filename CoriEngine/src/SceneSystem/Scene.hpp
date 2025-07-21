#pragma once
#include "Entity.hpp"
#include "Core/SelfFactory.hpp"
#include "Profiling/Trackable.hpp"
#include "Renderer/CameraController.hpp"
#include "EventSystem/Event.hpp"
#include "Physics/Physics.hpp"
#include "Renderer/Animator/AnimatorPool.hpp"

namespace Cori {
	namespace Graphics {
		struct QuadPrimitive;
		template<typename Primitive> requires  Utils::OneOf<Primitive, Cori::Graphics::QuadPrimitive>
		class PrimitivePool;
	}

	namespace Components {
		namespace Entity {
			struct RenderGroup;
		}
	}

	struct PoolStorage;

	template<typename... T>
	inline constexpr auto& Exclude = entt::exclude<T...>;

	class Scene : public Profiling::Trackable<Scene>, public SharedSeflFactory<Scene> {
	public:
		static bool PreCreateHook([[maybe_unused]] const std::string& name) { return true; }
		~Scene();

		bool OnBind(const EventCallbackFn& callback);
		bool OnUnbind();

		void OnUpdate(const double deltaTime);

		void OnTickUpdate(const float timeStep);

		Entity CreateEntity(const std::string& name);
		Entity CreateEntity();

		std::expected<void, const char*> AddEntityToCache(Entity entity);

		void GetEntityFromCache(const Utils::HashedTag64& tag);

		void DestroyEntity(Entity entity);

		Entity GetNamedEntity(const std::string& name);

		void SortRenderGroup();

		Entity EntityFromEnTT(const entt::entity& entity) {
			return Entity{ entt::handle{m_Registry, entity} };
		}

		// and const variants
		template<typename... T, typename... Args>
		auto View(Args&& ... args) {
			Entity::SetViewScene(this);
			return m_Registry.view<T...>(std::forward<Args>(args)...);
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

		CameraController ActiveCamera;
		
		Physics::PhysicsWorld PhysicsWorld;

		EventCallbackFn m_TriggerEventCallback;

		std::string m_Name;

		friend struct Components::Entity::StateMachine;

		friend struct Components::Entity::RenderGroup;

		std::unique_ptr<PoolStorage> m_pImpl;

		template<typename Primitive>
		Graphics::PrimitivePool<Primitive>& GetPoolForType();

		//std::tuple<
		//Graphics::PrimitivePool<Graphics::QuadPrimitive>
		//> m_PrimitivePools;

		//template<typename T>
		//Graphics::PrimitivePool<T>& GetPoolForType() {
		//	return std::get<Graphics::PrimitivePool<T>>(m_PrimitivePools);
		//}

		explicit Scene(const std::string& name);
	protected:
	private:

		void UpdateTransform();

		void UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, uint8_t parentLayer, bool parentTransformDirty, bool parentLayerDirty);

		void OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity);

		std::unordered_map<std::string, entt::handle> m_NamedEntities;


		entt::registry m_Registry;

		friend class Entity;
	};

}
