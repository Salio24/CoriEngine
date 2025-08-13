#include "Scene.hpp"
#include "Renderer/CameraController.hpp"
#include "Physics/Triggers/Trigger.hpp"
#include "Renderer/Renderer2D.hpp"

namespace Cori {

	Scene::Scene(const std::string& name) : m_Name(name) {
		AddContextComponent<Components::Scene::Camera>();
		m_ActiveCamera.BindCameraComponent(&GetContextComponent<Components::Scene::Camera>());
		CORI_CORE_DEBUG("Scene: '{0}' created.", m_Name);
		m_Registry.on_destroy<Components::Entity::Hierarchy>().connect<&Scene::OnHierarchyComponentDestroyed>(this);
		[[maybe_unused]] auto nameAndTagGroup = m_Registry.group<Components::Entity::Name, Components::Entity::Tag>();
	}

	Scene::~Scene() {
		m_Registry.clear();
		CORI_CORE_DEBUG("Scene: '{0}' destroyed.", m_Name);
	}

	Entity Scene::CreateEntity(const std::string& name, const Utility::HashedTag64& tag) {
		entt::entity entity = m_Registry.create();
		m_Registry.emplace<Components::Entity::Name>(entity, name);
		m_Registry.emplace<Components::Entity::Tag>(entity, tag);
		m_Registry.emplace<Components::Entity::Transform>(entity);
		m_Registry.emplace<Components::Entity::Hierarchy>(entity);
		auto& uuidComp = m_Registry.emplace<Components::Entity::UUID>(entity);
		m_UUIDToEntity.insert({uuidComp.m_UUID, entity});
		CORI_CORE_TRACE_TAGGED({"World", "Scene"}, "Created Entity With ID: {}, Version: {}, Name: {}, Tag: {}",entt::to_integral(entity), entt::to_version(entity), name, tag.GetDebugName());
		return Entity{ {m_Registry, entity} };
	}

	std::expected<void, const char*> Scene::AddEntityToCache(Entity entity, const Utility::StringHash32 key) {
		if (m_EntityCache.contains(key)) {
			return std::unexpected("Entry with the given key already exists, make sure you're not reusing the key. It's also possible (but very unlikely), that you got a hash collision, try to change the key a bit.");
		}
		m_EntityCache.insert({key, entity.GetRawEntity()});
		CORI_CORE_DEBUG_TAGGED({"World", "Scene"}, "Entity '{}' added to scene local cache, key: '{}'", entity.GetDebugData(), key);
		return{};
	}

	std::expected<Entity, const char*> Scene::GetEntityFromCache(const Utility::StringHash32 key) {
		if (!m_EntityCache.contains(key)) {
			return std::unexpected("No entity with the specified key found in scene local cache.");
		}
		if (!m_Registry.valid(m_EntityCache.at(key))) {
			m_EntityCache.erase(key);
			return std::unexpected("Entity at the specified key is invalid, removing this entry.");
		}
		return Entity{{m_Registry, m_EntityCache.at(key)}};
	}

	void Scene::RemoveEntityFromCache(const Utility::StringHash32 key) {
		if (m_EntityCache.contains(key)) {
			Entity entity = entt::handle{m_Registry, m_EntityCache.at(key)};
			m_EntityCache.erase(key);
			CORI_CORE_DEBUG_TAGGED({"World", "Scene"}, "Entity '{}' removed from scene local cache, key: '{}'", entity.GetDebugData(), key);
		}
		CORI_CORE_WARN_TAGGED({"World", "Scene"}, "Trying to remove an Entity from scene local cache, provided key: '{}', but cache has no such entry.", key);
	}

	std::expected<Entity, const char*> Scene::FindEntity(const std::string& name) {
		CORI_CORE_WARN_TAGGED({"World", "Scene"}, "Performing slow scene-wide search for entity named: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", name);
		auto view = m_Registry.view<Components::Entity::Name>();
		for (auto entity : view) {
			if (name == view.get<Components::Entity::Name>(entity).m_Name) {
				return Entity{{m_Registry, entity}};
			}
		}
		return std::unexpected("No entity found with the specified name.");
	}
	std::expected<Entity, const char*> Scene::FindEntity(const std::string& name, const Utility::HashedTag64& tag) {
		CORI_CORE_WARN_TAGGED({"World", "Scene"}, "Performing slow scene-wide search for entity with tag: '{}', named: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", tag.GetDebugName(), name);
		auto group = m_Registry.group<Components::Entity::Name, Components::Entity::Tag>();
		for (auto entity : group) {
			auto [nameComp , tagComp] = group.get<Components::Entity::Name, Components::Entity::Tag>(entity);
			if (name == nameComp.m_Name && tag == tagComp.m_Tag) {
				return Entity{{m_Registry, entity}};
			}
		}
		return std::unexpected("No entity found with the specified name and tag.");
	}

	void Scene::DestroyEntity(Entity entity) {
		if (!entity.IsValid()) { return; }
		if (entity.HasComponents<Components::Entity::Hierarchy>()) {
			entity.UnlinkFromParent();
		}
		auto& uuidComp = entity.GetComponents<Components::Entity::UUID>();
		m_UUIDToEntity.erase(uuidComp.m_UUID);
		m_Registry.destroy(entity.GetRawEntity());
	}

	void Scene::OnUpdate([[maybe_unused]] const double deltaTime) {
		CORI_PROFILE_FUNCTION();

		{
			CORI_PROFILE_SCOPE("Recursive transform update");
			UpdateTransform();
		}

		Renderer2D::BeginScene(GetContextComponent<Components::Scene::Camera>());
		Renderer2D::DrawScene(this);
		Renderer2D::FlushRenderQueues();
		Renderer2D::EndScene();
	}

	void Scene::OnTickUpdate(const float timeStep) {
		m_PhysicsWorld.Step(timeStep, 4);

		auto fsmv = m_Registry.view<Components::Entity::StateMachine>();

		for (auto entity : fsmv) {
			auto& fsm = fsmv.get<Components::Entity::StateMachine>(entity);
			fsm.Update(timeStep);
		}

		auto trigv = m_Registry.view<Components::Entity::Trigger>();

		// order is not enforced
		for (auto entity : trigv) {
			trigv.get<Components::Entity::Trigger>(entity).OnTickUpdate(timeStep);
		}

		b2SensorEvents sEvents = m_PhysicsWorld.GetSensorEvents();

		for (int i = 0; i < sEvents.beginCount; ++i)
		{
			b2SensorBeginTouchEvent* beginTouch = sEvents.beginEvents + i;

			Entity& visitor = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(beginTouch->visitorShapeId).GetBody().GetUserData())->m_Entity;

			Entity& trigger = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(beginTouch->sensorShapeId).GetBody().GetUserData())->m_Entity;

			trigger.GetComponents<Components::Entity::Trigger>().OnEnter(visitor);
		}

		for (int i = 0; i < sEvents.endCount; ++i)
		{
			b2SensorEndTouchEvent* endTouch = sEvents.endEvents + i;

			Entity& visitor = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(endTouch->visitorShapeId).GetBody().GetUserData())->m_Entity;

			Entity& trigger = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(endTouch->sensorShapeId).GetBody().GetUserData())->m_Entity;

			trigger.GetComponents<Components::Entity::Trigger>().OnExit(visitor);
		}

	}

	bool Scene::OnBind() {
		return true;
	}

	bool Scene::OnUnbind() {
		return true;
	}

	void Scene::UpdateTransform() {
		auto view = m_Registry.view<Components::Entity::Transform, Components::Entity::Hierarchy>();
		for (auto entity : view) {
			const auto& hierarchy = view.get<Components::Entity::Hierarchy>(entity);
			if (!m_Registry.valid(hierarchy.m_Parent)) {
				UpdateTransformRecursive(entity, glm::mat3(1.0f), 1, false, false);
			}
		}
	}
	void Scene::UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, uint8_t parentDepth, bool parentTransformDirty, bool parentDepthDirty) {
		auto& transform = m_Registry.get<Components::Entity::Transform>(entity);
		const bool transformDirty = transform.m_DirtyTransform || parentTransformDirty;
		const bool layerDirty = transform.m_DirtyDepth || parentDepthDirty;

		if (transformDirty) {
			transform.m_WorldTransform = parentTransform * transform.GetLocalTransform();
			transform.m_DirtyTransform = false;
		}
		if (layerDirty) {
			transform.m_WorldDepth = parentDepth + transform.GetLocalDepthOffset();
			transform.m_DirtyDepth = false;
		}

		const auto& hierarchy = m_Registry.get<Components::Entity::Hierarchy>(entity);
		entt::entity currentChildID = hierarchy.m_FirstChild;
		while (m_Registry.valid(currentChildID)) {
			UpdateTransformRecursive(currentChildID, transform.m_WorldTransform, transform.m_WorldDepth, transformDirty, layerDirty);
			currentChildID = m_Registry.get<Components::Entity::Hierarchy>(currentChildID).m_NextSibling;
		}
	}

	void Scene::OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity) {
		const auto& hierarchy = registry.get<Components::Entity::Hierarchy>(entity);
		entt::entity currentChildID = hierarchy.m_FirstChild;
		while (registry.valid(currentChildID)) {
			entt::entity nextChildID = registry.get<Components::Entity::Hierarchy>(currentChildID).m_NextSibling;
			registry.destroy(currentChildID); // This triggers a recursive call for grandchildren.
			currentChildID = nextChildID;
		}
	}

	// temporary need to use c++20 modules
}
