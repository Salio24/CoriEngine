#include "Trigger.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			Trigger::Trigger(Cori::Entity& trigger) {
				if (trigger.IsValid()) {
					auto& ud = trigger.AddComponent<Physics::BodyUserData>(trigger);
					auto& rb = trigger.GetComponents<Rigidbody>();
					rb.SetUserData(&ud);
				}
			}

			void Trigger::OnEnter(Cori::Entity& entity) {
				if (m_Behavior) {
					if (m_VisitorBuffer.size() > CORI_MAX_TRIGGER_VISITORS) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::Trigger }, "Trigger '{}': Exceeded maximum number of visitors ({}).", m_Behavior->GetDebugName(), CORI_MAX_TRIGGER_VISITORS);
						return;
					}
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::Trigger }, "Trigger '{}': Entity '{}' has entered.", m_Behavior->GetDebugName(), entity.GetDebugData());
					m_Behavior->OnEnter(entity);
					m_VisitorBuffer.add(entity);
				}
			}

			// order is not enforced
			void Trigger::OnTickUpdate(const float timeStep) {
				for (auto& visitor : m_VisitorBuffer) {
					if (m_Behavior) {
						m_Behavior->OnTickUpdate(visitor, timeStep);
					}
				}
			}

			void Trigger::OnExit(Cori::Entity& entity) {
				if (m_Behavior) {
					if (m_VisitorBuffer.remove(entity)) {
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::Trigger }, "Trigger '{}': Entity '{}' has exited.", m_Behavior->GetDebugName(), entity.GetDebugData());
						m_Behavior->OnExit(entity);
					}
				}
			}
		}
	}
}
