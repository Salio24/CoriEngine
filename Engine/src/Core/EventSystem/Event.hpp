#pragma once

namespace Cori {
	namespace Core {
		enum EventCategory {
			None = 0,
			EventCategoryApplication = 1 << 0,
			EventCategoryGameplay    = 1 << 1,
			EventCategoryInput       = 1 << 2,
			EventCategoryKeyboard    = 1 << 3,
			EventCategoryMouse       = 1 << 4,
			EventCategoryMouseButton = 1 << 5
		};

		/**
		 * @brief An abstract class that is ment to be used as a template for defining events.
		 */
		class Event {
			friend class EventDispatcher;
		public:
			virtual ~Event() = default;

			/**
			 * @brief Gives the return type of the Event.
			 * @note You shouldn't overload this, it is overloaded by EVENT_CLASS_TYPE macro!
			 * @return Type index of derived event type.
			 */
			[[nodiscard]] virtual constexpr std::type_index GetEventType() const = 0;


			/**
			 * @brief This will give you the string version of EventType.
			 * @note You shouldn't overload this, it is overloaded by EVENT_CLASS_TYPE macro!
			 * @return EventType name.
			 */
			[[nodiscard]] virtual const char* GetName() const = 0;

			/**
			 * @brief Gives the flags of particular Event.
			 * @note You shouldn't overload this, it is overloaded by EVENT_CLASS_CATEGORY macro!
			 * @return Flag variable that stores all the relevant EventCategory flags.
			 */
			[[nodiscard]] virtual uint32_t GetCategoryFlags() const = 0;


			/**
			 * @brief Returns the name of the Event.
			 * @note This is up to the user to overload, by default return the same thing GetName() does.
			 * @return Event name.
			 */
			[[nodiscard]] virtual std::string to_string() const { return GetName(); }


			/**
			 * @brief Checks if the Event is in specific category.
			 * @param category Bitmask that we need to check presence of.
			 * @return True is present, false otherwise.
			 */
			[[nodiscard]] bool IsInCategory(const EventCategory category) const {
				return GetCategoryFlags() & category;
			}

			bool m_Handled = false;
		};

		/**
		 * @brief Used in OnEvent methods to dispatch and handle Events.
		 */
		class EventDispatcher {
			template<typename T>
			using EventFn = std::function<bool(T&)>;
		public:
			explicit EventDispatcher(Event& event)
				: m_Event(event) {
			}

			/**
			 * @brief Binds dispatch function that will handle a specific Event type.
			 * @tparam T Event type to dispatch.
			 * @param func Lambda or function bind.
			 * @details If true is returned from the bound function the Event is considered handled, otherwise the said Event will be propagated down the LayerStack.
			 * @return Returns the same value that is returned in the bound function.
			 */
			template<typename T>
			bool Dispatch(EventFn<T> func) {
				if (m_Event.GetEventType() == std::type_index(typeid(T))) {
					m_Event.m_Handled = func(*static_cast<T*>(&m_Event));
					return true;
				}


				return false;
			}
		private:
			Event& m_Event;
		};

		/**
		 * @brief  Needed for CoriError.
		 */
		inline std::ostream& operator<<(std::ostream& os, const Event& e) {
			return os << e.to_string();
		}

		/**
		 * @brief Needed for fmt/spadlog.
		 */
		inline std::string format_as(const Event& e) {
			return std::string(e.to_string());
		}

		using EventCallbackFn = std::function<void(Event&)>;
	}
}


/**
 * @brief Assigns an EventType to the custom event type derived from the Event class. Use it in the public field of the derived class.
 * @param type Derived from Event typename.
 * @detals Example:
 * \n class PlayerDied final : public Cori::Core::Event
 * \n EVENT_CLASS_TYPE(PlayerDied)
 */
#define EVENT_CLASS_TYPE(type) constexpr std::type_index GetEventType() const override { return std::type_index(typeid(type)); }\
								const char* GetName() const override { return CORI_CLEAN_TYPE_NAME(type); }

/**
 * @brief Assigns an EventCategory to the custom event type derived from the Event class. Use it in the public field of the derived class.
 * @note Several EventCategories can be assigned to the derived class using an bitwise or operator.
 * @param category EventCategories to be assigned.
 */
#define EVENT_CLASS_CATEGORY(category) uint32_t GetCategoryFlags() const override { return category; }