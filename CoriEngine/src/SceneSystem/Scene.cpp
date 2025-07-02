#include "Scene.hpp"
#include "Renderer/Renderer2D.hpp"
#include "Renderer/CameraController.hpp"
#include "Physics/Triggers/Trigger.hpp"

namespace Cori {

	Scene::Scene(const std::string& name) : m_Name(name) {
		AddContextComponent<Components::Scene::Camera>();
		ActiveCamera.BindCameraComponent(&GetContextComponent<Components::Scene::Camera>());
		CORI_CORE_DEBUG("Scene: '{0}' created.", m_Name);
		auto renderGroup = m_Registry.group<Components::Entity::Render, Components::Entity::Sprite>();

	}

	Scene::~Scene() {
		m_Registry.clear();
		CORI_CORE_DEBUG("Scene: '{0}' destroyed.", m_Name);
	}

	Entity Scene::CreateEntity(const std::string& name) {
		CORI_CORE_ASSERT_FATAL(this != nullptr, "Called scene instance is null (instance pointer = nullptr), this causes undefined behavior, and this assert also relies on this undefined behavior and is not guarantied.");
		if (CORI_CORE_ASSERT_ERROR(!m_NamedEntities.contains(name), "Trying to create a named entity, but the specified name already exists in a hashmap, this is not permited, named entities should have exclusive names. Name: '{}'. Invalid entity returned.", name)) { return Entity{}; }

		entt::entity entity = m_Registry.create();
		m_Registry.emplace<Components::Entity::Name>(entity, name);
		m_NamedEntities.insert({ name, entt::handle{m_Registry, entity} });
		CORI_CORE_TRACE("Created Named Entity With ID: {0}, Version: {1}, Name: {2}",entt::to_integral(entity), entt::to_version(entity), name);
		return Entity{ {m_Registry, entity} };
	}

	Entity Scene::CreateEntity() {
		entt::entity entity = m_Registry.create();
		//CORI_CORE_TRACE("Created Unnamed Entity With ID: {0}, Version: {1}", entt::to_integral(entity), entt::to_version(entity));
		return Entity{ {m_Registry, entity} };

	}

	Entity Scene::GetNamedEntity(const std::string& name) {
		if (CORI_CORE_ASSERT_ERROR(m_NamedEntities.contains(name), "Named entity with name '{0}' doesn't exist, returned null entity.", name)) { return Entity{}; }
		return Entity{ m_NamedEntities.at(name) };
	}


	void Scene::SortRenderGroup() {
		auto renderGroup = m_Registry.group<Components::Entity::Render, Components::Entity::Sprite>();
		renderGroup.sort<Components::Entity::Sprite>([](const Components::Entity::Sprite& lhs, const Components::Entity::Sprite& rhs) {
			return reinterpret_cast<uint64_t>(lhs.m_Texture.get()) > reinterpret_cast<uint64_t>(rhs.m_Texture.get());
		});
	}

	void Scene::OnUpdate(const double deltaTime) {
		CORI_PROFILE_FUNCTION();

		Renderer2D::BeginBatch(GetContextComponent<Components::Scene::Camera>().m_ViewProjectionMatrix);

		auto renderGroup = m_Registry.group<Components::Entity::Render, Components::Entity::Sprite>();
		auto& camera = GetContextComponent<Components::Scene::Camera>();

		for (auto entity : renderGroup) {
			auto [renderComp, spriteComp] = renderGroup.get<Components::Entity::Render, Components::Entity::Sprite>(entity);
			if (renderComp.m_Visible) {
				if ((camera.m_CameraMinBound.x <= renderComp.m_Position.x + renderComp.m_Size.x && camera.m_CameraMaxBound.x >= renderComp.m_Position.x) && (camera.m_CameraMinBound.x <= renderComp.m_Position.y + renderComp.m_Size.y && camera.m_CameraMaxBound.y >= renderComp.m_Position.y)) {
					Renderer2D::DrawQuad(renderComp.m_Position, renderComp.m_Size, spriteComp.m_Texture, spriteComp.m_UVs, renderComp.m_Layer, renderComp.m_Flipped);
				}
			}
		}
		Renderer2D::EndBatch();
		
	}

	void Scene::OnTickUpdate(const float timeStep) {
		PhysicsWorld.Step(timeStep, 4);

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

		b2SensorEvents sEvents = PhysicsWorld.GetSensorEvents();

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

	bool Scene::OnBind(const EventCallbackFn& callback) {
		m_TriggerEventCallback = callback;
		return true;
	}

	bool Scene::OnUnbind() {
		m_TriggerEventCallback = EventCallbackFn();
		return true;
	}

}