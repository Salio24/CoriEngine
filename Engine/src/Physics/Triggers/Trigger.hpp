#pragma once
#include "TriggerBehaviour.hpp"
#include "WorldSystem/Components.hpp"
#include "Core/DataStructures/PackedArray.hpp"

#ifndef CORI_MAX_TRIGGER_VISITORS
	#define CORI_MAX_TRIGGER_VISITORS 4
#endif

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				class Trigger {
				public:
					Trigger() = default;
					explicit Trigger(World::Entity& trigger);

					void OnEnter(World::Entity& entity);

					void OnTickUpdate(const float timeStep);

					void OnExit(World::Entity& entity);

					template<typename Behavior>
					void SetBehavior() {
						static_assert(std::is_base_of_v<TriggerBehaviour, Behavior>, "Behavior must inherit from TriggerBehaviour");
						m_Behavior = std::make_unique<Behavior>();
						m_VisitorBuffer.clear();
					}

				private:
					Core::PackedArray<World::Entity, uint32_t, CORI_MAX_TRIGGER_VISITORS> m_VisitorBuffer;

					std::unique_ptr<TriggerBehaviour> m_Behavior{ nullptr };
				};
			}
		}
	}
}
