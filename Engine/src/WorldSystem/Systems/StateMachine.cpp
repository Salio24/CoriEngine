#include "StateMachine.hpp"
#include "StateSystem/StateMachine.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			void StateMachine::OnTickUpdate(Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();

				EntityView view = m_Owner.View<Components::Entity::StateMachine>(Exclude<Components::Entity::InactiveLocallyFlag>());

				for (const auto entity : view) {
					view.Get<Components::Entity::StateMachine>(entity).OnTickUpdate(gameTimer.GetTimestep());
				}
			}

			bool StateMachine::Create() {
				return true;
			}
		}
	}
}
