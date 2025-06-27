#include "TriggerManager.hpp"
#include "SceneManager.hpp"

namespace Cori {

	void TriggerManager::OnUpdate(const double deltaTime, std::shared_ptr<Scene> scene) {
		//auto playerView = scene->View<PlayerTagComponent, ColliderComponent, PositionComponent>();
		//auto triggerView = scene->View<TriggerComponent, ColliderComponent, PositionComponent>();
		//
		//for (auto player : playerView) {
		//	auto [pPos, pCol] = playerView.get<PositionComponent, ColliderComponent>(player);
		//	for (auto trigger : triggerView) {
		//		auto [tPos, tCol] = triggerView.get<PositionComponent, ColliderComponent>(trigger);
		//
		//		if (RectVsRect(pCol, pPos, tCol, tPos)) {
		//			GameTriggerStayEvent event(trigger, player, scene->m_TriggerEventCallback);
		//			m_EventCallback(event);
		//		}
		//	}
		//}
	}
}