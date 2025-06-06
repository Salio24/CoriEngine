#pragma once
#include "Scene.hpp"
#include "../EventSystem/GameEvents.hpp"

namespace Cori {
	class TriggerManager {
	public:

		using EventCallbackFn = std::function<void(Event&)>;

		static TriggerManager& Get() {
			static TriggerManager instance;
			return instance;
		}

		void OnUpdate(const double deltaTime, Scene* scene);
		void SetEventCallback(const EventCallbackFn& callback) { m_EventCallback = callback; }

		void Test() {
			test = true;

		}

		bool test{false};

	private:
		EventCallbackFn m_EventCallback;
	};
}