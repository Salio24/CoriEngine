#pragma once
#include "System.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class StateMachine final : public System {
			public:
				void OnTickUpdate(Core::GameTimer& gameTimer) override;

				bool Create();

				static constexpr SystemPriority Priority = 100;

			private:
				void OnStateMachineCreate(entt::registry& registry, entt::entity entity);
			};
		}
	}
}
