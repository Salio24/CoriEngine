#pragma once
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				class Trigger;
			}
		}

		class TriggerBehaviour {
		public:
			virtual ~TriggerBehaviour() = default;
			[[nodiscard]] virtual const char* GetDebugName() const { return "Unnamed Trigger"; }
		protected:
			friend Components::Entity::Trigger;
			virtual void OnEnter([[maybe_unused]] Entity& visitor, [[maybe_unused]] Entity& trigger) {}

			virtual void OnTickUpdate([[maybe_unused]] Entity& visitor, [[maybe_unused]] Entity& trigger, [[maybe_unused]] float timestep) {}

			virtual void OnExit([[maybe_unused]] Entity& visitor, [[maybe_unused]] Entity& trigger) {}


			// maybe add an onevent func 
		};
	}
}