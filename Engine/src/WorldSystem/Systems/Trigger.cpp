#include "Trigger.hpp"
#include "Physics/Triggers/Trigger.hpp"
#include "WorldSystem/Components.hpp"
#include "WorldSystem/Systems/Physics.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			void Trigger::OnTickUpdate(Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();

				auto system = m_Owner.GetSystem<PhysicsSystem>();
				if (system) {
					auto locked = system->lock();
						StaticEntityView view = m_Owner.StaticView<Components::Entity::Trigger>(Exclude<Components::Entity::InactiveLocallyFlag>());

						for (const auto entity : view) {
							view.Get<Components::Entity::Trigger>(entity).OnTickUpdate(gameTimer.GetTimestep());
						}

						auto [beginEvents, endEvents, beginCount, endCount] = locked->GetWorld().GetSensorEvents();

						for (int32_t i = 0; i < beginCount; ++i) {
							const b2SensorBeginTouchEvent* beginTouch = beginEvents + i;

							Entity& visitor = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(beginTouch->visitorShapeId).GetBody().GetUserData())->m_Entity;

							Entity& trigger = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(beginTouch->sensorShapeId).GetBody().GetUserData())->m_Entity;

							if (trigger.IsActiveGlobally()) {
								trigger.GetComponents<Components::Entity::Trigger>().OnEnter(visitor);
							}
						}

						for (int32_t i = 0; i < endCount; ++i) {
							const b2SensorEndTouchEvent* endTouch = endEvents + i;

							Entity& visitor = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(endTouch->visitorShapeId).GetBody().GetUserData())->m_Entity;

							Entity& trigger = static_cast<Physics::BodyUserData*>(static_cast<Physics::ShapeRef>(endTouch->sensorShapeId).GetBody().GetUserData())->m_Entity;

							if (trigger.IsActiveGlobally()) {
								trigger.GetComponents<Components::Entity::Trigger>().OnExit(visitor);
							}
						}
				}
			}

			bool Trigger::Create() {
				m_Owner.GetRegistry().on_construct<Physics::BodyUserData>().connect<&Trigger::OnBodyUserDataCreate>(this);
				m_Owner.GetRegistry().on_construct<Components::Entity::Trigger>().connect<&Trigger::OnTriggerCreate>(this);
				return true;
			}

			void Trigger::OnBodyUserDataCreate(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& bud = e.GetComponents<Physics::BodyUserData>();
				bud.m_Entity = e;
			}

			void Trigger::OnTriggerCreate(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& tr = e.GetComponents<Components::Entity::Trigger>();
				auto& ud = e.GetOrAddComponent<Physics::BodyUserData>();
				ud.m_Entity = e;
				auto& rb = e.GetComponents<Components::Entity::RigidBody>();
				rb.SetUserData(&ud);
				tr.m_Trigger = e;
			}
		}
	}
}
