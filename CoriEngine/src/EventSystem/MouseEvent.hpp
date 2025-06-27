#pragma once
#include "Event.hpp"
#include "Core/CoriMouseCodes.hpp"

namespace Cori {

	class MouseMovedEvent : public Event {
	public:
		MouseMovedEvent(int x, int y)
			: m_MouseX(x), m_MouseY(y) {
		}

		inline int GetX() const { return m_MouseX; }
		inline int GetY() const { return m_MouseY; }

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseMovedEvent: " << "(" << m_MouseX << ", " << m_MouseY << ")";
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseMoved)
		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	private:
		int m_MouseX{ 0 }, m_MouseY{ 0 };
	};

	class MouseScrolledEvent : public Event {
	public:
		MouseScrolledEvent(short xDirection, short yDirection)
			: m_xDirection(xDirection), m_yDirection(yDirection) {}

		inline short GetXOffset() const { return m_xDirection; }
		inline short GetYOffset() const { return m_yDirection; }

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseScrolledEvent: " << "(" << m_xDirection << ", " << m_yDirection << ")";
			return ss.str();
		}
		EVENT_CLASS_TYPE(MouseScrolled)
		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	private:
		short m_xDirection{ 0 }, m_yDirection{ 0 };
	};

	class MouseButtonEvent : public Event {
	public:
		inline CoriMouseCode GetMouseButton() const { return m_Button; }
		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryMouseButton | EventCategoryInput)
	protected:
		MouseButtonEvent(CoriMouseCode button)
			: m_Button(button) {}

		CoriMouseCode m_Button{ CORI_MOUSEBUTTON_UNKNOWN };
	};

	class MouseButtonPressedEvent : public MouseButtonEvent {
	public:
		MouseButtonPressedEvent(CoriMouseCode button)
			: MouseButtonEvent(button) {}

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseButtonPressedEvent: " << CoriGetKeyName(m_Button);
			return ss.str();
		}
		EVENT_CLASS_TYPE(MouseButtonPressed)
	};

	class MouseButtonReleasedEvent : public MouseButtonEvent {
	public:
		MouseButtonReleasedEvent(CoriMouseCode button)
			: MouseButtonEvent(button) {}

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent: " << CoriGetKeyName(m_Button);
			return ss.str();
		}
		EVENT_CLASS_TYPE(MouseButtonReleased)
	};
}