#pragma once
#include "Trigger.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			Trigger::Trigger(Cori::Entity& trigger) {
				if (trigger.IsValid()) {
					auto& ud = trigger.AddComponent<Cori::Physics::BodyUserData>(trigger);
					auto& rb = trigger.GetComponents<Cori::Components::Entity::Rigidbody>();
					rb.SetUserData(&ud);
				}
			}

			void Trigger::OnEnter(Cori::Entity& entity) {
				if (m_Behavior) {
					CORI_CORE_ASSERT_WARN(m_VisitorBuffer.size() < CORI_MAX_TRIGGER_VISITORS, "Trigger '{0}': Exceeded maximum number of visitors ({1}).", m_Behavior->GetDebugName(), CORI_MAX_TRIGGER_VISITORS);
					CORI_CORE_TRACE("Trigger '{0}': Entity '{1}' has entered.", m_Behavior->GetDebugName(), entity.GetDebuggingUID());
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
					CORI_CORE_TRACE("Trigger '{0}': Entity '{1}' has exited.", m_Behavior->GetDebugName(), entity.GetDebuggingUID());
					m_Behavior->OnExit(entity);
					m_VisitorBuffer.remove(entity);
				}
			}
		}
	}
}
