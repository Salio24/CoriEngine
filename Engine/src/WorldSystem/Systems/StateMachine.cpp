#include "StateMachine.hpp"
#include "StateSystem/StateMachine.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			void StateMachine::OnTickUpdate(Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();

				StaticEntityView view = m_Owner.StaticView<Components::Entity::StateMachine>(Exclude<Components::Entity::InactiveLocallyFlag>());

				for (const auto entity : view) {
					view.Get<Components::Entity::StateMachine>(entity).OnTickUpdate(gameTimer.GetTimestep());
				}
			}

			bool StateMachine::Create() {
				m_Owner.GetRegistry().on_construct<Components::Entity::StateMachine>().connect<&StateMachine::OnStateMachineCreate>(this);
				return true;
			}

			void StateMachine::OnStateMachineCreate(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& sm = e.GetComponents<Components::Entity::StateMachine>();
				sm.m_Owner = e;
			}
		}
	}
}
