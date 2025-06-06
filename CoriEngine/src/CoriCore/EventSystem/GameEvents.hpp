#pragma once
#include "Event.hpp"
#include "../ECS/Entity.hpp"

namespace Cori {
	class GameTriggerStayEvent : public Event {
	public:
		GameTriggerStayEvent(Entity triggerEntity, Entity otherEntity) : m_TriggerEntity(std::move(triggerEntity)), m_OtherEntity(std::move(otherEntity)) {}

		std::string ToString() const override {
			return "GameTriggerStayEvent";
		}

		inline Entity& GetTriggerEntity() {
			return m_TriggerEntity;
		}

		inline Entity& GetOtherEntity() {
			return m_OtherEntity;
		}

		EVENT_CLASS_TYPE(GameTriggerStay)
		EVENT_CLASS_CATEGORY(EventCategoryGameplay)
	private:

		Entity m_TriggerEntity;
		Entity m_OtherEntity;
	};


}