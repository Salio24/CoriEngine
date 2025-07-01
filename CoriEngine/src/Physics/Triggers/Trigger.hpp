#pragma once
#include "TriggerBehaviour.hpp"
#include "SceneSystem/Components.hpp"
#include "Core/DataStructures/PackedArray.hpp"

#ifndef CORI_MAX_TRIGGER_VISITORS
	#define CORI_MAX_TRIGGER_VISITORS 4
#endif

namespace Cori {
	namespace Components {
		namespace Entity {
			class Trigger {
			public:
				Trigger() = default;
				Trigger(Cori::Entity& trigger);

				void OnEnter(Cori::Entity& entity);

				void OnTickUpdate(const float timeStep);

				void OnExit(Cori::Entity& entity);

				template<typename Behavior>
				void SetBehavior() {
					static_assert(std::is_base_of_v<Cori::Physics::TriggerBehaviour, Behavior>, "Behavior must inherit from TriggerBehaviour");
					m_Behavior = std::make_unique<Behavior>();
					m_VisitorBuffer.clear();
				}

			private:
				PackedArray<Cori::Entity, uint8_t, CORI_MAX_TRIGGER_VISITORS> m_VisitorBuffer;

				std::unique_ptr<Cori::Physics::TriggerBehaviour> m_Behavior{ nullptr };
			};
		}
	}
}