#pragma once
#include "SceneSystem/Entity.hpp"

namespace Cori {
	namespace Physics {
		class TriggerBehaviour {
		public:
			virtual ~TriggerBehaviour() = default;

			virtual void OnEnter(Entity& visitor) {}

			virtual void OnTickUpdate(Entity& visitor, float timestep) {}

			virtual void OnExit(Entity& visitor) {}

			virtual const char* GetDebugName() const { return "Unnamed Trigger"; }

			// maybe add an onevent func 
		};
	}
}