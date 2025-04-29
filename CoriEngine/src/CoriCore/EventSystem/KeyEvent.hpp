#pragma once
#include "Event.hpp"
#include "CoriKeycodes.hpp"

namespace Cori {
	class KeyEvent : public Event {
	public:
		inline CoriKeycode GetKeyCode() const { return m_KeyCode; }
		EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
	protected:
		KeyEvent(CoriKeycode keycode)
			: m_KeyCode(keycode) {
		}
		CoriKeycode m_KeyCode{ CORI_KEY_UNKNOWN };
	};

	class KeyPressedEvent : public KeyEvent {
	public:
		KeyPressedEvent(CoriKeycode keycode, bool repeat)
			: KeyEvent(keycode), m_Repeat(repeat) {
		}

		inline bool IsRepeated() const { return m_Repeat; }

		std::string ToString() const override {
			std::stringstream ss;
			ss << "KeyPressedEvent: " << CoriGetKeyName(m_KeyCode) << " ( Repeated: " << std::boolalpha << m_Repeat << " )";
			return ss.str();
		}
		EVENT_CLASS_TYPE(KeyPressed)
	private:
		bool m_Repeat{ 0 };
	};

	class KeyReleasedEvent : public KeyEvent {
	public:
		KeyReleasedEvent(CoriKeycode keycode)
			: KeyEvent(keycode) {
		}
		std::string ToString() const override {
			std::stringstream ss;
			ss << "KeyReleasedEvent: " << CoriGetKeyName(m_KeyCode);
			return ss.str();
		}
		EVENT_CLASS_TYPE(KeyReleased)
	};
}