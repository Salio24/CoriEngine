#pragma once
#include "TriggerBehaviour.hpp"
#include "WorldSystem/Components.hpp"
#include "Core/DataStructures/PackedArray.hpp"
#include "WorldSystem/Systems/Trigger.hpp"
#include "TriggerBehaviour.hpp"

#ifndef CORI_MAX_TRIGGER_VISITORS
	#define CORI_MAX_TRIGGER_VISITORS 4
#endif

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				/**
				 * @brief A Trigger components is used in combination with RigidBody component used to respond to an entity getting into a specific area that has the Trigger.
				 * @details For a Trigger to work you need to attach a shape (From now on I will refer to shapes created like this as a Trigger zone) to the RigidBody body component with the following params:
				 * ```cpp
				 * filter.categoryBits = Cori::Physics::CollisionBits::SensorBit;
				 * isSensor = true;
				 * enableSensorEvents = true;
				 * ```
				 * Trigger can register and update a limited amount of entities inside of it at the same time, this is controlled by ```CORI_MAX_TRIGGER_VISITORS``` define, default is 4, you can increase it if you need to.
				 * \n When an entity enters the Trigger zone it will. All the function calls I'm referring to below is calls to the current TriggerBehaviour instance. (Lets say N is the tick entity exits the Trigger zone)
				 * \n Tick 1: Fire ```OnEnter```
				 * \n Tick 2 to (N - 1): Fire ```OnTickUpdate```
				 * \n Tick N: Fire OnTickUpdate and then ```OnExit```
				 * \n The order in which the OnTickUpdate fires for each Entity currently in the Trigger zone is not enforced or guaranteed to be in any particular order.
				 */
				class Trigger {
				public:
					Trigger() = default;

					/**
					 * @brief Sets or changes the current Trigger behavior/script.
					 * @tparam Behavior A behavior type derived from TriggerBehaviour.
					 * @return The raw ptr to the created Behavior instance.
					 */
					template<std::derived_from<TriggerBehaviour> Behavior>
					Behavior* SetBehavior() {
						m_Behavior = std::make_unique<Behavior>();
						m_BehaviourType = typeid(Behavior);
						m_VisitorBuffer.clear();
						return static_cast<Behavior*>(m_Behavior.get());
					}

					/**
					 * @brief Checks if a specific behaviour is currently assigned to the trigger.
					 * @tparam Behavior Behavior to check the presence of.
					 * @return True if current behaviour is the one you provided, false otherwise.
					 */
					template<std::derived_from<TriggerBehaviour> Behavior>
					[[nodiscard]] bool HasBehaviour() const {
						return m_BehaviourType == std::type_index(typeid(Behavior));
					}

					/**
					 * @brief Retries the pointer to the active behaviour instance.
					 * @tparam Behavior A behavior type derived from TriggerBehaviour. This needs to be the same that you used when setting the behavior with SetBehavior().
					 * @return Expected object with Behavior* on success, or a CoriError<std::type_index> object on failure, std::type_index stored inside the CoriError is the type_index of actual behavior currently used.
					 */
					template<std::derived_from<TriggerBehaviour> Behavior>
					std::expected<Behavior*, Core::CoriError<std::type_index>> GetBehavior() {
						if (std::type_index(typeid(Behavior)) != m_BehaviourType) {
							return std::unexpected(Core::CoriError<std::type_index>(std::format("Failed to retrieve a pointer to <{}> type of TriggerBehavior, type of TriggerBehavior stored in the Trigger is <{}>, type mismatch.", CORI_CLEAN_TYPE_NAME(Behavior), CORI_DEMANGLE(m_BehaviourType.name())), "Stored Type", m_BehaviourType));
						}

						return static_cast<Behavior*>(m_Behavior.get());
					}

				private:
					friend Systems::Trigger;
					void OnEnter(World::Entity& entity);

					void OnTickUpdate(const float timeStep);

					void OnExit(World::Entity& entity);
					Core::PackedArray<World::Entity, uint32_t, CORI_MAX_TRIGGER_VISITORS> m_VisitorBuffer;
					std::unique_ptr<TriggerBehaviour> m_Behavior{ nullptr };
					std::type_index m_BehaviourType{ typeid(nullptr) };
					World::Entity m_Trigger;
				};
			}
		}
	}
}
