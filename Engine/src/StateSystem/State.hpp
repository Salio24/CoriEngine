#pragma once
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				class StateMachine;
			}
		}



		class EntityState {
		public:
			virtual ~EntityState() = default;

			virtual void OnEnter([[maybe_unused]] Entity& owner) {}

			virtual void OnTickUpdate([[maybe_unused]] Entity& owner, [[maybe_unused]] float timestep) {}

			virtual void OnExit([[maybe_unused]] Entity& owner, [[maybe_unused]] const std::type_info& nextStateType) {}

			[[nodiscard]] virtual const char* GetDebugName() const { return "Unnamed State"; }

			// maybe add an onevent func
		};
	}
}