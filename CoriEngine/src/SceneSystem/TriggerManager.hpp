#pragma once
#include "Scene.hpp"
#include "EventSystem/GameEvents.hpp"
#include "Components.hpp"

namespace Cori {
	class TriggerManager {
	public:

		static TriggerManager& Get() {
			static TriggerManager instance;
			return instance;
		}

		void OnUpdate(const double deltaTime, std::shared_ptr<Scene> scene);
		void SetEventCallback(const EventCallbackFn& callback) { m_EventCallback = callback; }

	private:
		EventCallbackFn m_EventCallback;
	};
}