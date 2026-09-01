#pragma once
#include "Event.hpp"

namespace Cori {
	namespace Core {
		class WindowPixelResizeEvent final : public Event {
		public:
			WindowPixelResizeEvent(const uint32_t width, const uint32_t height)
				: m_Width(width), m_Height(height) {}

			[[nodiscard]] uint32_t GetWidth() const { return m_Width; }
			[[nodiscard]] uint32_t GetHeight() const { return m_Height; }

			[[nodiscard]] std::string to_string() const override {
				return std::format("WindowPixelResizeEvent: ({}, {})", m_Width, m_Height);
			}

			EVENT_CLASS_TYPE(WindowPixelResizeEvent)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
		private:
			uint32_t m_Width{ 0 };
			uint32_t m_Height{ 0 };
		};

		class WindowLogicalResizeEvent final : public Event {
		public:
			WindowLogicalResizeEvent(const uint32_t width, const uint32_t height)
				: m_Width(width), m_Height(height) {}

			[[nodiscard]] uint32_t GetWidth() const { return m_Width; }
			[[nodiscard]] uint32_t GetHeight() const { return m_Height; }

			[[nodiscard]] std::string to_string() const override {
				return std::format("WindowLogicalResizeEvent: ({}, {})", m_Width, m_Height);
			}

			EVENT_CLASS_TYPE(WindowLogicalResizeEvent)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
		private:
			uint32_t m_Width{ 0 };
			uint32_t m_Height{ 0 };
		};

		class WindowDisplayScaleChangedEvent final : public Event {
		public:
			explicit WindowDisplayScaleChangedEvent(const float scale)
				: m_Scale(scale) {}

			[[nodiscard]] float GetScale() const { return m_Scale; }

			[[nodiscard]] std::string to_string() const override {
				return std::format("WindowDisplayScaleChangedEvent: ({})", m_Scale);
			}

			EVENT_CLASS_TYPE(WindowDisplayScaleChangedEvent)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
		private:
			float m_Scale{ 0 };
		};

		class WindowCloseEvent final : public Event {
		public:
			WindowCloseEvent() = default;

			[[nodiscard]] std::string to_string() const override {
				return "WindowCloseEvent";
			}

			EVENT_CLASS_TYPE(WindowCloseEvent)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
		};
	}
}