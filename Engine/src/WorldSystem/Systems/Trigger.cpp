#include "Trigger.hpp"
#include "Physics/Triggers/Trigger.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			void Trigger::OnTickUpdate(Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();

				EntityView view = m_Owner.View<Components::Entity::Trigger>(Exclude<Components::Entity::InactiveLocallyFlag>());

				for (const auto entity : view) {
					view.Get<Components::Entity::Trigger>(entity).OnTickUpdate(gameTimer.GetTimestep());
				}

				auto [beginEvents, endEvents, beginCount, endCount] = m_Owner.GetContextComponent<Components::Scene::PhysicsWorld>().GetSensorEvents();

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

			bool Trigger::Create() {
				return true;
			}
		}
	}
}
