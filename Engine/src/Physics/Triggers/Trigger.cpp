#include "Trigger.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				Trigger::Trigger(World::Entity& trigger) {
					if (CORI_CORE_VERIFY(trigger.IsValid(), "An invalid entity was passed to the Trigger, always pass the same entity you're adding a Trigger to. This can blow up any second now.")) {}
					else {
						auto& ud = trigger.GetOrAddComponent<Physics::BodyUserData>();
						ud.m_Entity = trigger;
						auto& rb = trigger.GetComponents<RigidBody>();
						rb.SetUserData(&ud);
						m_Trigger = trigger;
					}
				}

				void Trigger::OnEnter(World::Entity& entity) {
					if (m_Behavior) {
						if (m_VisitorBuffer.size() > CORI_MAX_TRIGGER_VISITORS) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::Trigger }, "Trigger '{}': Exceeded maximum number of visitors ({}).", m_Behavior->GetDebugName(), CORI_MAX_TRIGGER_VISITORS);
							return;
						}
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::Trigger }, "Trigger '{}': Entity '{}' has entered.", m_Behavior->GetDebugName(), entity.GetDebugData());
						m_Behavior->OnEnter(entity, m_Trigger);
						m_VisitorBuffer.push_back(entity);
					}
				}

				void Trigger::OnTickUpdate(const float timeStep) {
					for (auto& visitor : m_VisitorBuffer) {
						if (m_Behavior) {
							m_Behavior->OnTickUpdate(visitor, m_Trigger, timeStep);
						}
					}
				}

				void Trigger::OnExit(World::Entity& entity) {
					if (m_Behavior) {
						if (m_VisitorBuffer.remove(entity)) {
							CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::Trigger }, "Trigger '{}': Entity '{}' has exited.", m_Behavior->GetDebugName(), entity.GetDebugData());
							m_Behavior->OnExit(entity, m_Trigger);
						}
					}
				}
			}
		}
	}
}
