#pragma once

namespace Cori {

	/**
	 * @brief Severity levels, ordered from least to most severe.
	 * @details eOff is not a level a message can carry, it only exists as a tag floor that suppresses everything.
	 */
	enum class LogLevel : uint8_t {
		eTrace = 0,
		eDebug = 1,
		eInfo = 2,
		eWarm = 3,
		eError = 4,
		eFatal = 5,
		eOff = 6
	};

	/**
	 * @brief Human readable name of a level, for the console and for settings serialization.
	 */
	[[nodiscard]] constexpr const char* LogLevelName(const LogLevel level) noexcept {
		switch (level) {
		case LogLevel::eTrace: return "Trace";
		case LogLevel::eDebug: return "Debug";
		case LogLevel::eInfo:  return "Info";
		case LogLevel::eWarm:  return "Warn";
		case LogLevel::eError: return "Error";
		case LogLevel::eFatal: return "Fatal";
		case LogLevel::eOff:   return "Off";
		}
		return "Unknown";
	}

	/**
	 * @brief A named logging category that carries its own runtime level floor.
	 * @details A message tagged with this category is dropped unless its severity is at least the floor.
	 * \n Every tag links itself into a global intrusive list on construction so the editor can enumerate
	 * categories without anyone maintaining ids. The list head is constant initialized, so a tag defined in
	 * any translation unit can register safely regardless of static initialization order.
	 * \n Tags carry an optional parent purely so the console can draw them as a tree. The filter itself does
	 * not walk parents, hierarchy comes from call sites listing the whole chain, e.g.
	 * { Graphics::Self, Graphics::Vulkan::Self, Graphics::Vulkan::TextureManager }. Raising the floor on any
	 * link of that chain silences everything below it.
	 * @note A LogTag must have static storage duration, it is never unlinked from the registry. The floor is a relaxed atomic.
	 */
	class LogTag {
	public:
		explicit LogTag(const std::string_view name, const LogTag* parent = nullptr) noexcept : m_Name(name), m_Parent(parent) {
			LogTag* head = s_Head.load(std::memory_order_relaxed);
			m_Next = head;
			while (!s_Head.compare_exchange_weak(head, this, std::memory_order_release, std::memory_order_relaxed)) {
				m_Next = head;
			}
		}

		LogTag(const LogTag&) = delete;
		LogTag& operator=(const LogTag&) = delete;
		LogTag(LogTag&&) = delete;
		LogTag& operator=(LogTag&&) = delete;

		[[nodiscard]] std::string_view GetName() const { return m_Name; }

		[[nodiscard]] const LogTag* GetParent() const { return m_Parent; }

		[[nodiscard]] LogLevel GetFloor() const { return m_Floor.load(std::memory_order_relaxed); }

		/**
		 * @brief Raises or lowers the severity floor of this category.
		 * @details Safe to call from any thread.
		 */
		void SetFloor(const LogLevel floor) const { m_Floor.store(floor, std::memory_order_relaxed); }

		template<typename F>
		static void ForEach(F&& f) {
			for (const LogTag* tag = s_Head.load(std::memory_order_acquire); tag != nullptr; tag = tag->m_Next) {
				f(*tag);
			}
		}


		/**
		 * @brief Sets the floor of every registered tag at once.
		 */
		static void SetAllFloors(const LogLevel floor) noexcept {
			for (const LogTag* tag = s_Head.load(std::memory_order_acquire); tag != nullptr; tag = tag->m_Next) {
				tag->SetFloor(floor);
			}
		}

		[[nodiscard]] static size_t Count() noexcept {
			size_t count = 0;
			for (const LogTag* tag = s_Head.load(std::memory_order_acquire); tag != nullptr; tag = tag->m_Next) {
				++count;
			}
			return count;
		}

	private:
		std::string_view m_Name;
		const LogTag* m_Parent{ nullptr };
		LogTag* m_Next{ nullptr };
		mutable std::atomic<LogLevel> m_Floor{ LogLevel::eTrace };

		static inline constinit std::atomic<LogTag*> s_Head{ nullptr };
	};

	using LogTagRef = std::reference_wrapper<const LogTag>;

	[[nodiscard]] inline bool PassesTagFilter(const LogLevel level, const std::initializer_list<LogTagRef> tags) noexcept {
		for (const LogTagRef tag : tags) {
			if (level < tag.get().GetFloor()) {
				return false;
			}
		}
		return true;
	}
}
