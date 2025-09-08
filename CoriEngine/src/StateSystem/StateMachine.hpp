#pragma once
#include "State.hpp"
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				class StateMachine {
				public:
					// ReSharper disable once CppParameterMayBeConst
					explicit StateMachine(World::Entity owner) : m_Owner(owner), m_CurrentState(nullptr), m_LastState(nullptr) {
						CORI_CORE_ASSERT(m_Owner.IsValid(), "StateMachine owner Entity is not valid!");
					}

					~StateMachine() {
						if (m_CurrentState) {
							m_CurrentState->OnExit(m_Owner, typeid(nullptr));
						}
					}

					StateMachine(const StateMachine&) = delete;
					StateMachine& operator=(const StateMachine&) = delete;

					template<typename StateType, typename... Args>
					void Register(Args&&... args) {
						const std::type_index stateID(typeid(StateType));

						if (m_States.contains(stateID)) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: State type '{}' already registered.", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(*m_CurrentState));
							return;
						}

						m_States[stateID] = std::make_unique<StateType>(std::forward<Args>(args)...);
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: Registered state '{}'", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(StateType));
					}

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
						m_CurrentState->OnEnter(m_Owner);
					}

					void OnTickUpdate(const float timeStep) {
						if (m_CurrentState) {
							m_CurrentState->OnTickUpdate(m_Owner, timeStep);
						}
					}

					template<std::derived_from<EntityState> StateType>
					bool IsInState() const {
						if (!m_CurrentState) {
							return false;
						}

						return typeid(StateType) == typeid(*m_CurrentState);
					}

					template<std::derived_from<EntityState> StateType>
					void SetStateIfNotInState() {
						if (!IsInState<StateType>()) {
							return SetState<StateType>();
						}
					}

					EntityState* GetCurrentState() const {
						CORI_CORE_ASSERT("Trying to retrieve current state, but current state is null. Always make sure you set some initial state before using fsm.")
						return m_CurrentState;
					}

					EntityState* GetLastState() const {
						CORI_CORE_ASSERT("Trying to retrieve last state, but last state is null. Always make sure you set some initial state before using fsm.")
						return m_LastState;
					}

					World::Entity GetOwner() const {
						return m_Owner;
					}

				private:
					World::Entity m_Owner;
					EntityState* m_CurrentState;
					EntityState* m_LastState;

					std::unordered_map<std::type_index, std::unique_ptr<EntityState>> m_States;
				};
			}
		}
	}
}