#include "TriggerManager.hpp"
#include "SceneManager.hpp"

namespace Cori {

	bool TriggerManager::RectVsRect(const ColliderComponent& col1, const PositionComponent& pos1, const ColliderComponent& col2, const PositionComponent& pos2) {
		return (pos1.Position.x + col1.Offset.x < pos2.Position.x + col2.Offset.x + col2.Size.x && pos1.Position.x + col1.Offset.x + col1.Size.x > pos2.Position.x + col2.Offset.x &&
				pos1.Position.y + col1.Offset.y < pos2.Position.y + col2.Offset.y + col2.Size.y && pos1.Position.y + col1.Offset.y + col1.Size.y > pos2.Position.y + col2.Offset.y);
	}

	void TriggerManager::OnUpdate(const double deltaTime, std::shared_ptr<Scene> scene) {
		auto playerView = scene->View<PlayerTagComponent, ColliderComponent, PositionComponent>();
		auto triggerView = scene->View<TriggerComponent, ColliderComponent, PositionComponent>();

		for (auto player : playerView) {
			auto [pPos, pCol] = playerView.get<PositionComponent, ColliderComponent>(player);
			for (auto trigger : triggerView) {
				auto [tPos, tCol] = triggerView.get<PositionComponent, ColliderComponent>(trigger);
				if (RectVsRect(pCol, pPos, tCol, tPos)) {
					GameTriggerStayEvent event(scene->EntityFromEnTT(trigger), scene->EntityFromEnTT(player), scene->m_TriggerEventCallback);
					m_EventCallback(event);
				}
			}
		}
	}
}