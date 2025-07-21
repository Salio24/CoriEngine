#include "Scene.hpp"
#include "Renderer/Renderer2D.hpp"
#include "Renderer/CameraController.hpp"
#include "Physics/Triggers/Trigger.hpp"
#include "Renderer/Renderer2DNew.hpp"

namespace Cori {

	struct PoolStorage {
		std::tuple<
			Graphics::PrimitivePool<Graphics::QuadPrimitive>
		> m_PrimitivePools;

		PoolStorage(Scene* parentScene) : m_PrimitivePools(
				Graphics::PrimitivePool<Graphics::QuadPrimitive>(CORI_GRAPHICS_QUAD_POOL_INITIAL_SIZE, parentScene)
			)
		{}
	};

	//Scene::Scene(const std::string& name) : m_Name(name), m_PrimitivePools(Graphics::PrimitivePool<Graphics::QuadPrimitive>(CORI_GRAPHICS_QUAD_POOL_INITIAL_SIZE, this)) {
	Scene::Scene(const std::string& name) : m_Name(name), m_pImpl(std::make_unique<PoolStorage>(this)) {
		AddContextComponent<Components::Scene::Camera>();
		ActiveCamera.BindCameraComponent(&GetContextComponent<Components::Scene::Camera>());
		CORI_CORE_DEBUG("Scene: '{0}' created.", m_Name);
		m_Registry.on_destroy<Components::Entity::HierarchyComponent>().connect<&Scene::OnHierarchyComponentDestroyed>(this);
		//auto renderGroup = m_Registry.group<Components::Entity::Render, Components::Entity::Sprite>();
	}

	template<typename Primitive>
	Graphics::PrimitivePool<Primitive>& Scene::GetPoolForType() {
		return std::get<Graphics::PrimitivePool<Primitive>>(m_pImpl->m_PrimitivePools);
	}

	Scene::~Scene() {
		m_Registry.clear();
		CORI_CORE_DEBUG("Scene: '{0}' destroyed.", m_Name);
	}

	Entity Scene::CreateEntity(const std::string& name) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-undefined-compare"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtautological-undefined-compare"
#endif
		CORI_CORE_ASSERT_FATAL(this != nullptr, "Called scene instance is null (instance pointer == nullptr), this causes undefined behavior, and this assert also relies on this undefined behavior and is not guarantied.");
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
		if (CORI_CORE_ASSERT_ERROR(!m_NamedEntities.contains(name), "Trying to create a named entity, but the specified name already exists in a hashmap, this is not permited, named entities should have exclusive names. Name: '{}'. Invalid entity returned.", name)) { return Entity{}; }
		CORI_CORE_DEBUG("");
		entt::entity entity = m_Registry.create();
		//m_Registry.emplace<Components::Entity::Name>(entity, name);
		m_NamedEntities.insert({ name, entt::handle{m_Registry, entity} });
		CORI_CORE_TRACE("Created Named Entity With ID: {0}, Version: {1}, Name: {2}",entt::to_integral(entity), entt::to_version(entity), name);
		return Entity{ {m_Registry, entity} };
	}

	Entity Scene::CreateEntity() {
		entt::entity entity = m_Registry.create();
		//CORI_CORE_TRACE("Created Unnamed Entity With ID: {0}, Version: {1}", entt::to_integral(entity), entt::to_version(entity));
		return Entity{ {m_Registry, entity} };

	}

	std::expected<void, const char*> Scene::AddEntityToCache(Entity entity) {

	}

	void Scene::GetEntityFromCache(const Utils::HashedTag64& tag) {

	}

	void Scene::DestroyEntity(Entity entity) {
		if (!entity.IsValid()) { return; }
		if (entity.HasComponents<Components::Entity::HierarchyComponent>()) {
			entity.UnlinkFromParent();
		}
		m_Registry.destroy(entity.GetHandle());
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

	void Scene::OnUpdate([[maybe_unused]] const double deltaTime) {
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

		Test::Renderer2D::BeginScene(GetContextComponent<Components::Scene::Camera>());

		Test::Renderer2D::BeginInstancedSet();

		// move to renderer method: drawscene
		for (auto& quad : GetPoolForType<Graphics::QuadPrimitive>()) {
			if ((quad.m_States & (Graphics::QuadPrimitive::Options::Visible | Graphics::QuadPrimitive::Options::Valid)) == (Graphics::QuadPrimitive::Options::Visible | Graphics::QuadPrimitive::Options::Valid)) {
				if (quad.IsSemiTransparent()) {
					Test::Renderer2D::SubmitTransparentQuad(quad);
					continue;
				}
				Test::Renderer2D::DrawQuadInstanced(quad);
			}
		}

		Test::Renderer2D::EndInstancedSet();

		Test::Renderer2D::EndScene();
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

	void Scene::UpdateTransform() {
		auto view = m_Registry.view<Components::Entity::TransformComponent, Components::Entity::HierarchyComponent>();
		for (auto entity : view) {
			const auto& hierarchy = view.get<Components::Entity::HierarchyComponent>(entity);
			if (!m_Registry.valid(hierarchy.m_Parent)) {
				UpdateTransformRecursive(entity, glm::mat3(1.0f), 1, false, false);
			}
		}
	}
	void Scene::UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, uint8_t parentLayer, bool parentTransformDirty, bool parentLayerDirty) {
		auto& transform = m_Registry.get<Components::Entity::TransformComponent>(entity);
		const bool transformDirty = transform.m_DirtyTransform || parentTransformDirty;
		const bool layerDirty = transform.m_DirtyLayer || parentLayerDirty;

		if (transformDirty) {
			transform.m_WorldTransform = parentTransform * transform.GetLocalTransform();
			transform.m_DirtyTransform = false;
		}
		if (layerDirty) {
			transform.m_WorldLayer = parentLayer + transform.GetLocalLayer();
			transform.m_DirtyLayer = false;
		}

		const auto& hierarchy = m_Registry.get<Components::Entity::HierarchyComponent>(entity);
		entt::entity currentChildID = hierarchy.m_FirstChild;
		while (m_Registry.valid(currentChildID)) {
			UpdateTransformRecursive(currentChildID, transform.m_WorldTransform, transform.m_WorldLayer, transformDirty, layerDirty);
			currentChildID = m_Registry.get<Components::Entity::HierarchyComponent>(currentChildID).m_NextSibling;
		}
	}

	void Scene::OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity) {
		const auto& hierarchy = registry.get<Components::Entity::HierarchyComponent>(entity);
		entt::entity currentChildID = hierarchy.m_FirstChild;
		while (registry.valid(currentChildID)) {
			entt::entity nextChildID = registry.get<Components::Entity::HierarchyComponent>(currentChildID).m_NextSibling;
			registry.destroy(currentChildID); // This triggers a recursive call for grandchildren.
			currentChildID = nextChildID;
		}

	}

	// temporary need to use c++20 modules
	template Graphics::PrimitivePool<Graphics::QuadPrimitive>& Scene::GetPoolForType<Graphics::QuadPrimitive>();
}
