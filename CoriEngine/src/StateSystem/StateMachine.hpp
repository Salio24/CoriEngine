#pragma once

#include "State.hpp"
#include "SceneSystem/Entity.hpp"

namespace Cori {
	class StateMachine {
	public:
		StateMachine(Entity owner) : m_Owner(owner), m_CurrentState(nullptr) {
			CORI_CORE_ASSERT_ERROR(m_Owner.IsValid(), "StateMachine owner Entity is not valid!");
		}

		~StateMachine() {
			if (m_CurrentState) {
				m_CurrentState->OnExit(m_Owner, this);
			}
		}

		StateMachine(const StateMachine&) = delete;
		StateMachine& operator=(const StateMachine&) = delete;
		StateMachine(StateMachine&&) = default;
		StateMachine& operator=(StateMachine&&) = default;

		// should states even have ctor args?
		template<typename StateType, typename... Args>
		void RegisterState(Args&&... args) {
			static_assert(std::is_base_of<State, StateType>::value, "StateType must derive from State");
			std::type_index state_id(typeid(StateType));

			if (CORI_CORE_ASSERT_WARN(!m_States.count(state_id), "StateMachine for Entity {0}: State type '{1}' already registered.", m_Owner.GetDebuggingUID(), typeid(StateType).name())) { return; }

			m_States[state_id] = std::make_unique<StateType>(std::forward<Args>(args)...);
			CORI_CORE_TRACE("StateMachine for Entity {0}: Registered state '{1}'.", m_Owner.GetDebuggingUID(), typeid(StateType).name());
		}

		template<typename StateType>
		void ChangeState() {
			static_assert(std::is_base_of<State, StateType>::value, "StateType must derive from State");
			std::type_index next_state_id(typeid(StateType));
			auto it = m_States.find(next_state_id);

			if (CORI_CORE_ASSERT_ERROR(it != m_States.end(), "StateMachine for Entity {0}: Attempted to change to unregistered state type '{1}'.", m_Owner.GetDebuggingUID(), typeid(StateType).name())) { return; }

			State* nextStateRawPtr = it->second.get();

			if (m_CurrentState == nextStateRawPtr) {
				return;
			}

			if (m_CurrentState) {
				CORI_CORE_TRACE("StateMachine for Entity {0}: Exiting state '{1}'.", m_Owner.GetDebuggingUID(), typeid(*m_CurrentState).name());
				m_CurrentState->OnExit(m_Owner, this);
			}

			m_CurrentState = nextStateRawPtr;
			CORI_CORE_TRACE("StateMachine for Entity {0}: Entering state '{1}'.", m_Owner.GetDebuggingUID(), typeid(*m_CurrentState).name());
			m_CurrentState->OnEnter(m_Owner, this);
		}

		void Update(float timeStep) {

			if (CORI_CORE_ASSERT_WARN(m_Owner.IsValid(), "Update called on an FSM with an invalid owner Entity {0}. Disabling FSM.", m_Owner.GetDebuggingUID())) { m_CurrentState = nullptr; return; }

			if (m_CurrentState) {
				m_CurrentState->OnUpdate(m_Owner, this, timeStep);
			}
		}

		template<typename StateType>
		bool IsInState() const {
			static_assert(std::is_base_of<State, StateType>::value, "StateType must derive from State");

			if (!m_CurrentState) {
				return false;
			}

			return typeid(StateType) == typeid(*m_CurrentState);
		}

		State* GetCurrentState() const {
			return m_CurrentState;
		}

		Entity GetOwner() const {
			return m_Owner;
		}

	private:
		Entity m_Owner;
		State* m_CurrentState;
		std::unordered_map<std::type_index, std::unique_ptr<State>> m_States;
	};
}