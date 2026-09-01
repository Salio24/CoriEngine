#pragma once
#include "Event.hpp"
#include "Core/CoriMouseCodes.hpp"

namespace Cori {
	namespace Core {
		class MouseMovedEvent final : public Event {
		public:
			MouseMovedEvent(const int32_t x, const int32_t y)
				: m_MouseX(x), m_MouseY(y) {
			}

			[[nodiscard]] int32_t GetX() const { return m_MouseX; }
			[[nodiscard]] int32_t GetY() const { return m_MouseY; }

			[[nodiscard]] std::string to_string() const override {
				return std::string("MouseMovedEvent: (") + std::to_string(m_MouseX) + std::string(", ") + std::to_string(m_MouseY) + std::string(")");
			}

			EVENT_CLASS_TYPE(MouseMovedEvent)
			EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
		private:
			int32_t m_MouseX{ 0 };
			int32_t m_MouseY{ 0 };
		};

		class MouseScrolledEvent final : public Event {
		public:
			MouseScrolledEvent(const int16_t xDirection, const int16_t yDirection)
				: m_xDirection(xDirection), m_yDirection(yDirection) {}

			int16_t GetXOffset() const { return m_xDirection; }
			int16_t GetYOffset() const { return m_yDirection; }

			std::string to_string() const override {
				return std::string("MouseScrolledEvent: (") + std::to_string(m_xDirection) + std::string(", ") + std::to_string(m_yDirection) + std::string(")");
			}
			EVENT_CLASS_TYPE(MouseScrolledEvent)
			EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
		private:
			int16_t m_xDirection{ 0 };
			int16_t m_yDirection{ 0 };
		};

		class MouseButtonEvent : public Event {
		public:
			CoriMouseKeycode GetMouseButton() const { return m_Button; }
			EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryMouseButton | EventCategoryInput)
		protected:
			explicit MouseButtonEvent(const CoriMouseKeycode button)
				: m_Button(button) {}

			CoriMouseKeycode m_Button{ CORI_MOUSEBUTTON_UNKNOWN };
		};

		class MouseButtonPressedEvent final : public MouseButtonEvent {
		public:
			explicit MouseButtonPressedEvent(const CoriMouseKeycode button)
				: MouseButtonEvent(button) {}

			std::string to_string() const override {
				return std::string("MouseButtonPressedEvent: ") + CoriGetKeyName(m_Button);
			}
			EVENT_CLASS_TYPE(MouseButtonEvent)
		};

		class MouseButtonReleasedEvent final : public MouseButtonEvent {
		public:
			explicit MouseButtonReleasedEvent(const CoriMouseKeycode button)
				: MouseButtonEvent(button) {}

			std::string to_string() const override {
				return std::string("MouseButtonReleasedEvent: ") + CoriGetKeyName(m_Button);

			}
			EVENT_CLASS_TYPE(MouseButtonReleasedEvent)
		};
	}
}