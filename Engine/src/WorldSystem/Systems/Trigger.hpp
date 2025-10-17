#pragma once
#include "System.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			/**
			 * @brief System responsible for handling physical triggers.
			 * @details Requires Scene to have Physics system registered. Scene doesn't have this system registered by default.
			 */
			class Trigger final : public System {
				public:

				void OnTickUpdate(Core::GameTimer& gameTimer) override;

				bool Create();

				static constexpr SystemPriority Priority = 300;
			private:
				void OnBodyUserDataCreate(entt::registry& registry, entt::entity entity);

				void OnTriggerCreate(entt::registry& registry, entt::entity entity);
			};
		}
	}
}
