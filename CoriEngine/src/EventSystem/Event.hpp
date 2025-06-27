#pragma once 
#include <spdlog/spdlog.h>

namespace Cori {
	enum class EventType {
		None = 0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		GameTriggerEntered, GameTriggerExit, GameTriggerStay, GameUserDefinedEvent,
		AppTick, AppUpdate, AppRender,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

	enum EventCategory {
		None = 0,
		EventCategoryApplication = 1 << 0,
		EventCategoryGameplay    = 1 << 1,
		EventCategoryInput       = 1 << 2,
		EventCategoryKeyboard    = 1 << 3,
		EventCategoryMouse       = 1 << 4,
		EventCategoryMouseButton = 1 << 5
	};

	class Event {
		friend class EventDispatcher;
	public:
		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }

		inline bool IsInCategory(EventCategory category) {
			return GetCategoryFlags() & category;
		}
		inline bool IsOfType(EventType type) {
			return GetEventType() == type;
		}
		bool m_Handeled = false;
	};

	class EventDispatcher {
		template<typename T>
		using EventFn = std::function<bool(T&)>;
	public:
		EventDispatcher(Event& event)
			: m_Event(event) {
		}

		template<typename T>
		bool Dispatch(EventFn<T> func) {
			if (m_Event.GetEventType() == T::GetStaticType()) {
				m_Event.m_Handeled = func(*(T*)&m_Event);
				return true;
			}
			return false;
		}
	private:
		Event& m_Event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e) {
		return os << e.ToString();
	}

	inline std::string format_as(const Event& e) {
		return e.ToString();
	}

	using EventCallbackFn = std::function<void(Event&)>;
}


#define EVENT_CLASS_TYPE(type) static ::Cori::EventType GetStaticType() { return ::Cori::EventType::type; }\
								virtual ::Cori::EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }