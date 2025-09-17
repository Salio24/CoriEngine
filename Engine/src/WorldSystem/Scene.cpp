#include "Scene.hpp"
#include "Graphics/CameraController.hpp"
#include "Physics/Triggers/Trigger.hpp"
#include "Graphics/Renderer2D.hpp"
#include "Graphics/Animator/QuadAnimatorNew.hpp"
#include "StateSystem/StateMachine.hpp"

namespace Cori {
	namespace World {
		std::shared_ptr<Scene> Scene::Create(std::string name) {
			return std::shared_ptr<Scene>(new Scene(std::move(name)));
		}

		Scene::Scene(std::string name) : m_Name(std::move(name)) {
			AddContextComponent<Components::Scene::Camera>();
			m_ActiveCamera.BindCameraComponent(&GetContextComponent<Components::Scene::Camera>());
			CORI_CORE_INFO_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Scene: '{}' created.", m_Name);
			m_Registry.on_destroy<Components::Entity::Hierarchy>().connect<&Scene::OnHierarchyComponentDestroyed>(this);
			[[maybe_unused]] auto nameAndTagGroup = m_Registry.group<Components::Entity::Name, Components::Entity::Tag>();
		}

		Scene::~Scene() {
			m_Registry.clear();
			CORI_CORE_INFO_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Scene: '{}' destroyed.", m_Name);
		}

		Entity Scene::CreateEntity(const std::string& name, const Utility::HashedTag64& tag) {
			entt::entity entity = m_Registry.create();
			auto& nameComp = m_Registry.emplace<Components::Entity::Name>(entity);
			nameComp.m_Name = name;
			m_Registry.emplace<Components::Entity::Tag>(entity, tag);
			//m_Registry.emplace<Components::Entity::Transform>(entity);
			m_Registry.emplace<Components::Entity::Hierarchy>(entity);
			const auto& uuidComp = m_Registry.emplace<Components::Entity::UUID>(entity);
			m_UUIDToEntity.insert({ uuidComp.m_UUID, entity });
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Created Entity With ID: {}, Version: {}, Name: {}, Tag: {}", entt::to_integral(entity), entt::to_version(entity), name, tag.GetDebugName());
			Entity e = entt::handle{m_Registry, entity};
			e.AddComponent<Components::Entity::Transform>(e);
			return e;
		}

		std::expected<void, Core::CoriError<>> Scene::AddEntityToCache(const Entity entity, const Utility::StringHash32 key) {
			if (m_EntityCache.contains(key)) {
				return std::unexpected(Core::CoriError("Entry with the given key already exists, make sure you're not reusing the key. It's also possible (but very unlikely), that you got a hash collision, try to change the key a bit."));
			}
			m_EntityCache.insert({ key, entity.GetRawEntity() });
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Entity '{}' added to scene local cache, key: '{}'", entity.GetDebugData(), key);
			return{};
		}

		std::expected<Entity, Core::CoriError<>> Scene::GetEntityFromCache(const Utility::StringHash32 key) {
			if (!m_EntityCache.contains(key)) {
				return std::unexpected(Core::CoriError("No entity with the specified key found in scene local cache."));
			}
			if (!m_Registry.valid(m_EntityCache.at(key))) {
				m_EntityCache.erase(key);
				return std::unexpected(Core::CoriError("Entity at the specified key is invalid, removing this entry."));
			}
			return Entity{ { m_Registry, m_EntityCache.at(key) } };
		}

		void Scene::RemoveEntityFromCache(const Utility::StringHash32 key) {
			if (m_EntityCache.contains(key)) {
				const Entity entity = entt::handle{ m_Registry, m_EntityCache.at(key) };
				m_EntityCache.erase(key);
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Entity '{}' removed from scene local cache, key: '{}'", entity.GetDebugData(), key);
			}
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Trying to remove an Entity from scene local cache, provided key: '{}', but cache has no such entry.", key);
		}

		std::expected<Entity, Core::CoriError<>> Scene::FindEntity(const std::string& name) {
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Performing slow scene-wide search for entity named: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", name);
			const auto view = m_Registry.view<Components::Entity::Name>();
			for (auto entity : view) {
				if (name == view.get<Components::Entity::Name>(entity).m_Name) {
					return Entity{ { m_Registry, entity } };
				}
			}
			return std::unexpected(Core::CoriError("No entity found with the specified name."));
		}
		std::expected<Entity, Core::CoriError<>> Scene::FindEntity(const std::string& name, const Utility::HashedTag64& tag) {
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Performing slow scene-wide search for entity with tag: '{}', named: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", tag.GetDebugName(), name);
			const auto group = m_Registry.group<Components::Entity::Name, Components::Entity::Tag>();
			for (auto entity : group) {
				auto [nameComp , tagComp] = group.get<Components::Entity::Name, Components::Entity::Tag>(entity);
				if (name == nameComp.m_Name && tag == tagComp.m_Tag) {
					return Entity{ { m_Registry, entity } };
				}
			}
			return std::unexpected(Core::CoriError("No entity found with the specified name and tag."));
		}

		std::vector<Entity> Scene::GetEntitiesWithTag(const Utility::HashedTag64& tag) {
			std::vector<Entity> entities;
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Performing slow scene-wide collection of all entities with tag: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", tag.GetDebugName());
			const auto view = m_Registry.view<Components::Entity::Tag>();
			for (const auto entity : view) {
				auto& tagComp = view.get<Components::Entity::Tag>(entity);
				if (tag == tagComp.m_Tag) {
					entities.emplace_back(entt::handle{ m_Registry, entity });
				}
			}
			return entities;
		}

		void Scene::DestroyEntity(Entity entity) {
			if (!entity.IsValid()) { return; }
			if (entity.HasComponents<Components::Entity::Hierarchy>()) {
				entity.UnlinkFromParent();
			}
			const auto& uuidComp = entity.GetComponents<Components::Entity::UUID>();
			m_UUIDToEntity.erase(uuidComp.m_UUID);
			m_Registry.destroy(entity.GetRawEntity());
		}

		void Scene::OnUpdate([[maybe_unused]] const double deltaTime) {
			CORI_PROFILE_FUNCTION();

			{
				CORI_PROFILE_SCOPE("Recursive transform update");
				UpdateTransform();
			}

			Graphics::Renderer2D::SubmitScene(this);
			Graphics::Renderer2D::EndFrame(GetContextComponent<Components::Scene::Camera>());
		}

		void Scene::OnTickUpdate(const float timeStep) {
			m_PhysicsWorld.Step(timeStep, 4);

			EntityView fsmv = View<Components::Entity::StateMachine>(Exclude<Components::Entity::InactiveLocallyFlag>());

			for (const auto entity : fsmv) {
				fsmv.Get<Components::Entity::StateMachine>(entity).OnTickUpdate(timeStep);
			}

			EntityView animv = View<Components::Entity::QuadAnimator>(Exclude<Components::Entity::InactiveLocallyFlag>());

			for (const auto entity : animv) {
				animv.Get<Components::Entity::QuadAnimator>(entity).OnTickUpdate();
			}

			EntityView trigv = View<Components::Entity::Trigger>(Exclude<Components::Entity::InactiveLocallyFlag>());

			for (const auto entity : trigv) {
				trigv.Get<Components::Entity::Trigger>(entity).OnTickUpdate(timeStep);
			}

			auto [beginEvents, endEvents, beginCount, endCount] = m_PhysicsWorld.GetSensorEvents();

			for (int32_t i = 0; i < beginCount; ++i)
			{
				const b2SensorBeginTouchEvent* beginTouch = beginEvents + i;

				Entity& visitor = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(beginTouch->visitorShapeId).GetBody().GetUserData())->m_Entity;

				Entity& trigger = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(beginTouch->sensorShapeId).GetBody().GetUserData())->m_Entity;

				if (trigger.IsActiveGlobally()) {
					trigger.GetComponents<Components::Entity::Trigger>().OnEnter(visitor);
				}
			}

			for (int32_t i = 0; i < endCount; ++i)
			{
				const b2SensorEndTouchEvent* endTouch = endEvents + i;

				Entity& visitor = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(endTouch->visitorShapeId).GetBody().GetUserData())->m_Entity;

				Entity& trigger = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(endTouch->sensorShapeId).GetBody().GetUserData())->m_Entity;

				if (trigger.IsActiveGlobally()) {
					trigger.GetComponents<Components::Entity::Trigger>().OnExit(visitor);
				}
			}

		}

		// ReSharper disable once CppMemberFunctionMayBeStatic
		bool Scene::OnBind() {
			return true;
		}

		// ReSharper disable once CppMemberFunctionMayBeStatic
		bool Scene::OnUnbind() {
			return true;
		}

		void Scene::UpdateTransform() {
			//const auto view = m_Registry.view<Components::Entity::Transform, Components::Entity::Hierarchy>();
			//for (const auto entity : view) {
			//	const auto& hierarchy = view.get<Components::Entity::Hierarchy>(entity);
			//	if (!m_Registry.valid(hierarchy.m_Parent)) {
			//		UpdateTransformRecursive(entity, glm::mat3(1.0f), 1, false, false);
			//	}
			//}

			const auto view1 = m_Registry.view<Components::Entity::Internal::DirtyTransformFlag>();

			// 2. This loop runs only a handful of times per frame in a typical scene.
			for (const auto entity : view1) {
				UpdateTransformRecursive(entity, glm::mat3(1.0f), 1, false, false);
			}
			m_Registry.clear<Components::Entity::Internal::DirtyTransformFlag>();
		}
		void Scene::UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, const uint8_t parentDepth, const bool parentTransformDirty, const bool parentDepthDirty) {
			auto& transform = m_Registry.get<Components::Entity::Transform>(entity);
			const bool transformDirty = transform.m_DirtyTransform || parentTransformDirty;
			const bool layerDirty = transform.m_DirtyDepth || parentDepthDirty;

			if (transformDirty) {
				if (!transform.GetDetachedState()) {
					transform.m_WorldTransform = parentTransform * transform.GetLocalTransform();
					transform.m_LastParentTransform = parentTransform;
				} else {
					transform.m_WorldTransform = transform.m_LastParentTransform * transform.GetLocalTransform();
				}
				transform.m_DirtyTransform = false;
			}
			if (layerDirty) {
				int16_t unclamped = parentDepth + transform.GetLocalDepthOffset();
				if (unclamped < 0 || unclamped > 255) {
					uint8_t clamped = static_cast<uint8_t>(std::clamp(unclamped, static_cast<int16_t>(0), static_cast<int16_t>(255)));
					CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Final calculated depth for Entity '{}' is '{}' which is outside of allowed range [0, 255], it will be clamped to '{}'", Entity{ { m_Registry, entity } }.GetDebugData(), unclamped, clamped);
					transform.m_WorldDepth = clamped;
					transform.m_DirtyDepth = false;
				} else {
					transform.m_WorldDepth = static_cast<uint8_t>(unclamped);
					transform.m_DirtyDepth = false;
				}
			}
			const auto& hierarchy = m_Registry.get<Components::Entity::Hierarchy>(entity);
			entt::entity currentChild = hierarchy.m_FirstChild;
			while (m_Registry.valid(currentChild)) {
				UpdateTransformRecursive(currentChild, transform.m_WorldTransform, transform.m_WorldDepth, transformDirty, layerDirty);
				currentChild = m_Registry.get<Components::Entity::Hierarchy>(currentChild).m_NextSibling;
			}

		}

		void Scene::OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity) {
			const auto& hierarchy = registry.get<Components::Entity::Hierarchy>(entity);
			entt::entity currentChild = hierarchy.m_FirstChild;
			while (registry.valid(currentChild)) {
				const entt::entity nextChild = registry.get<Components::Entity::Hierarchy>(currentChild).m_NextSibling;
				registry.destroy(currentChild); // This triggers a recursive call for grandchildren.
				currentChild = nextChild;
			}
		}
	}
}
