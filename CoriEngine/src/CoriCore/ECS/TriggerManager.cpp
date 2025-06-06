#include "TriggerManager.hpp"
#include "SceneManager.hpp"

namespace Cori {

	void TriggerManager::OnUpdate(const double deltaTime, Scene* scene) {
		if (test) {
			auto trigger = SceneManager::ActiveScene::GetNamedEntity("Trigger Entity");
			auto other = SceneManager::ActiveScene::GetNamedEntity("Other Entity");

			GameTriggerStayEvent event(trigger, other);

			m_EventCallback(event);

			test = false;
		}
	}
}