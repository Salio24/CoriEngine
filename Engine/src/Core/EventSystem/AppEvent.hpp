#pragma once
#include "Event.hpp"

namespace Cori {
	namespace Core {
		class WindowResizeEvent final : public Event {
		public:
			WindowResizeEvent(const uint32_t width, const uint32_t height)
				: m_Width(width), m_Height(height) {}

			[[nodiscard]] uint32_t GetWidth() const { return m_Width; }
			[[nodiscard]] uint32_t GetHeight() const { return m_Height; }

			[[nodiscard]] std::string ToString() const override {
				return std::string("WindowResizeEvent: (") + std::to_string(m_Width) + ", " + std::to_string(m_Height) + std::string(")");
			}

			EVENT_CLASS_TYPE(WindowResize)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
		private:
			uint32_t m_Width{ 0 };
			uint32_t m_Height{ 0 };
		};

		class WindowCloseEvent final : public Event {
		public:
			WindowCloseEvent() = default;

			[[nodiscard]] std::string ToString() const override {
				return "WindowCloseEvent";
			}

			EVENT_CLASS_TYPE(WindowClose)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
		};

		class AppRenderEvent final : public Event {
		public:
			AppRenderEvent() = default;

			[[nodiscard]] std::string ToString() const override {
				return "AppRenderEvent";
			}

			EVENT_CLASS_TYPE(AppRender)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
		};
	}
}