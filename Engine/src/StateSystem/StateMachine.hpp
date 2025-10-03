#pragma once
#include "EntityState.hpp"
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class StateMachine;
		}

		namespace Components {
			namespace Entity {
				/**
				 * @brief A StateMachine component is used in combination with custom objects derived from EntityState.
				 * @details You declare several types deriver from EntityState and implement the state logic you need, then before using states you need to register them with the StateMachine by using Register<T>() method.
				 * \n Let's say out Entity is in a state A then it changed state to B, then at tick N it entered state C. The tick sequence of this will be as follows:
				 * \n Entity changes it's state form A to B, tick 1: OnTickUpdate of A is called one last time, then OnExit of A is called, then OnEnter of B is called.
				 * \n Entity stays in state B, ticks 2 - (N - 1): Fire OnTickUpdate of B.
				 * \n Entity leaves state B and enters state C, tick N: OnTickUpdate of B is fired one last time, then OnExit of B is called, then OnEnder of C is called.
				 * @note Always make sure to set some initial state before actually using the FSM.
				 */
				class StateMachine {
				public:
					StateMachine() = default;

					~StateMachine() {
						if (m_CurrentState) {
							m_CurrentState->OnExit(m_Owner, typeid(nullptr));
						}
					}

					StateMachine(const StateMachine&) = delete;
					StateMachine& operator=(const StateMachine&) = delete;
					StateMachine(StateMachine&&) = delete;
					StateMachine& operator=(StateMachine&&) = delete;

					/**
					 * @brief Registers an EntityState with a StateMachine, you need to register a state before using it.
					 * @tparam StateType State type to register.
					 * @tparam Args Deduced automatically, no need to specify.
					 * @param args Arguments to the constructor of the StateType to use.
					 */
					template<std::derived_from<EntityState> StateType, typename... Args>
					void Register(Args&&... args) {
						const std::type_index stateID(typeid(StateType));

						if (m_States.contains(stateID)) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: State type '{}' already registered.", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(*m_CurrentState));
							return;
						}

						m_States[stateID] = std::make_unique<StateType>(std::forward<Args>(args)...);
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: Registered state '{}'", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(StateType));
					}

					/**
					 * @brief Changes the current state to the specified one.
					 * @tparam StateType State to change to.
					 */
					template<std::derived_from<EntityState> StateType>
					void SetState() {
						const std::type_index nextStateID(typeid(StateType));

						if (CORI_CORE_CHECK(m_States.contains(nextStateID), "StateMachine for Entity - {}: Attempted to change to unregistered state type '{}', register the State first!", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(StateType))) { return; }

						EntityState* nextStateRawPtr = m_States.at(nextStateID).get();

						if (m_CurrentState == nextStateRawPtr) {
							return;
						}

						if (m_CurrentState) {
							CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: Exiting state '{}'", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(*m_CurrentState));
							m_CurrentState->OnExit(m_Owner, typeid(*nextStateRawPtr));
						}

						if (m_LastState) {
							m_LastState = m_CurrentState;
						} else {
							m_LastState = nextStateRawPtr;
						}

						m_CurrentState = nextStateRawPtr;
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: Entering state '{}'", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(*m_CurrentState));
						m_CurrentState->OnEnter(m_Owner, typeid(*m_LastState));
					}

					void OnTickUpdate(const float timeStep) {
						if (m_CurrentState) {
							m_CurrentState->OnTickUpdate(m_Owner, timeStep);
						}
					}

					/**
					 * @brief Cheks if an Entity is in a specific state.
					 * @tparam StateType State to check for.
					 * @return True if is currently in StateType, false otherwise.
					 */
					template<std::derived_from<EntityState> StateType>
					bool IsInState() const {
						if (!m_CurrentState) {
							return false;
						}

						return typeid(StateType) == typeid(*m_CurrentState);
					}

					/**
					 * @brief Sets an Entity to the state only if it is not currently in that state.
					 * @tparam StateType State to change to.
					 */
					template<std::derived_from<EntityState> StateType>
					void SetStateIfNotInState() {
						if (!IsInState<StateType>()) {
							return SetState<StateType>();
						}
					}

					/**
					 * @brief Gets the pointer to the current state. Can be used to get the type_info of the current state.
					 * @return Pointer of their current state instance.
					 */
					EntityState* GetCurrentState() const {
						CORI_CORE_ASSERT(m_CurrentState, "Trying to retrieve current state, but current state is null. Always make sure you set some initial state before using fsm.")
						return m_CurrentState;
					}

					/**
					 * @brief Gets the pointer to the last state. Can be used to get the type_info of the last state.
					 * @return Pointer of their last state instance.
					 */
					EntityState* GetLastState() const {
						CORI_CORE_ASSERT(m_LastState, "Trying to retrieve last state, but last state is null. Always make sure you set some initial state before using fsm.")
						return m_LastState;
					}

				private:
					friend Systems::StateMachine;
					World::Entity m_Owner;
					EntityState* m_CurrentState{ nullptr };
					EntityState* m_LastState{ nullptr };

					std::unordered_map<std::type_index, std::unique_ptr<EntityState>> m_States;
				};
			}
		}
	}
}