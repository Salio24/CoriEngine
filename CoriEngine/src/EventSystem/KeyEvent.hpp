#pragma once
#include "Event.hpp"
#include "Core/CoriKeycodes.hpp"

namespace Cori {
	class KeyEvent : public Event {
	public:
		[[nodiscard]] CoriKeycode GetKeyCode() const { return m_KeyCode; }
		EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
	protected:
		explicit KeyEvent(const CoriKeycode keycode)
			: m_KeyCode(keycode) {
		}
		CoriKeycode m_KeyCode{ CORI_KEY_UNKNOWN };
	};

	class KeyPressedEvent final : public KeyEvent {
	public:
		KeyPressedEvent(const CoriKeycode keycode, const bool repeat)
			: KeyEvent(keycode), m_Repeat(repeat) {
		}

		[[nodiscard]] bool IsRepeated() const { return m_Repeat; }

		[[nodiscard]] std::string ToString() const override {
			return std::string("KeyPressedEvent: ") + CoriGetKeyName(m_KeyCode) + std::string(" ( Repeated: ") + Logger::BoolAlpha(m_Repeat) + std::string(" )");
		}
		EVENT_CLASS_TYPE(KeyPressed)
	private:
		bool m_Repeat{ false };
	};

	class KeyReleasedEvent final : public KeyEvent {
	public:
		explicit KeyReleasedEvent(const CoriKeycode keycode)
			: KeyEvent(keycode) {
		}

		[[nodiscard]] std::string ToString() const override {
			return std::string("KeyReleasedEvent: ") + CoriGetKeyName(m_KeyCode);
		}
		EVENT_CLASS_TYPE(KeyReleased)
	};
}