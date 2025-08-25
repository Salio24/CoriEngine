#pragma once
#include "State.hpp"
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			class StateMachine {
			public:
				// ReSharper disable once CppParameterMayBeConst
				explicit StateMachine(Cori::Entity owner) : m_Owner(owner), m_CurrentState(nullptr) {
					CORI_CORE_ASSERT(m_Owner.IsValid(), "StateMachine owner Entity is not valid!");
				}

				~StateMachine() {
					if (m_CurrentState) {
						m_CurrentState->OnExit(m_Owner, this);
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

				template<std::derived_from<State> StateType>
				void SetState() {
					const std::type_index nextStateID(typeid(StateType));

					if (CORI_CORE_CHECK(m_States.contains(nextStateID), "StateMachine for Entity - {}: Attempted to change to unregistered state type '{}', register the State first!", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(StateType))) { return; }

					State* nextStateRawPtr = m_States.at(nextStateID).get();

					if (m_CurrentState == nextStateRawPtr) {
						return;
					}

					if (m_CurrentState) {
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: Exiting state '{}'", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(*m_CurrentState));
						m_CurrentState->OnExit(m_Owner, this);
					}

					m_CurrentState = nextStateRawPtr;
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::StateMachine }, "StateMachine for Entity - {}: Entering state '{}'", m_Owner.GetDebugData(), CORI_CLEAN_TYPE_NAME(*m_CurrentState));
					m_CurrentState->OnEnter(m_Owner, this);
				}

				void OnTickUpdate(const float timeStep) {
					if (m_CurrentState) {
						m_CurrentState->OnTickUpdate(m_Owner, this, timeStep);
					}
				}

				template<std::derived_from<State> StateType>
				bool IsInState() const {
					if (!m_CurrentState) {
						return false;
					}

					return typeid(StateType) == typeid(*m_CurrentState);
				}

				template<std::derived_from<State> StateType>
				void SetStateIfNotInState() {
					if (!IsInState<StateType>()) {
						return SetState<StateType>();
					}
				}

				State* GetCurrentState() const {
					return m_CurrentState;
				}

				Cori::Entity GetOwner() const {
					return m_Owner;
				}

			private:
				Cori::Entity m_Owner;
				State* m_CurrentState;
				std::unordered_map<std::type_index, std::unique_ptr<State>> m_States;
			};
		}
	}
}