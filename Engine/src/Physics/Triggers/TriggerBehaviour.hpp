#pragma once
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				class Trigger;
			}
		}

		/**
		 * @brief An abstract class meant to be used with Trigger component. Derive from it to create a trigger behaviour/script.
		 */
		class TriggerBehaviour {
		public:
			virtual ~TriggerBehaviour() = default;
			[[nodiscard]] virtual const char* GetDebugName() const { return "Unnamed Trigger"; }
		protected:
			friend Components::Entity::Trigger;
			/**
			 * @brief Will be fired when an entity enter the Trigger zone.
			 * @param visitor Visitor Entity.
			 * @param trigger Trigger Entity. Owner of the Trigger object the behavior is bound to.
			 */
			virtual void OnEnter([[maybe_unused]] Entity& visitor, [[maybe_unused]] Entity& trigger) {}

			/**
			 * @brief Fired every tick when an Entity is inside the Trigger zone. Expect for the very first tick the Entity entered the zone.
			 * @param visitor Visitor Entity.
			 * @param trigger Trigger Entity. Owner of the Trigger object the behavior is bound to.
			 * @param timestep Do i need to explain this?
			 */
			virtual void OnTickUpdate([[maybe_unused]] Entity& visitor, [[maybe_unused]] Entity& trigger, [[maybe_unused]] float timestep) {}

			/**
			 * @brief Fired very last tick when an Entity leaves the Trigger zone.
			 * @param visitor Visitor Entity.
			 * @param trigger Trigger Entity. Owner of the Trigger object the behavior is bound to.
			 */
			virtual void OnExit([[maybe_unused]] Entity& visitor, [[maybe_unused]] Entity& trigger) {}


			// maybe add an onevent func 
		};
	}
}