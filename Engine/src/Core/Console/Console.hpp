#pragma once
#include "CVar.hpp"

namespace Cori {
	namespace Core {

		using ConsoleArgs = std::span<const std::string_view>;

		struct ConsoleCompletion {
			std::string_view m_Text;
			std::string m_Detail;
		};

		/**
		 * @brief What a console command hands back: a line to print on success, or an error to print instead.
		 */
		using ConsoleResult = std::expected<std::string, std::string>;

		/**
		 * @brief A named callable exposed to the console prompt.
		 * @details Registers itself into a global intrusive list on construction, the same trick LogTag uses.
		 * @note A ConsoleCommand must have static storage duration, it is never unlinked. Use a plain function pointer rather than a capturing lambda so registration allocates nothing before main.
		 */
		class ConsoleCommand {
		public:
			using Fn = ConsoleResult (*)(ConsoleArgs);

			ConsoleCommand(const std::string_view name, const std::string_view help, const Fn fn) noexcept : m_Name(name), m_Help(help), m_Fn(fn) {
				ConsoleCommand* head = s_Head.load(std::memory_order_relaxed);
				m_Next = head;
				while (!s_Head.compare_exchange_weak(head, this, std::memory_order_release, std::memory_order_relaxed)) {
					m_Next = head;
				}
			}

			ConsoleCommand(const ConsoleCommand&) = delete;
			ConsoleCommand& operator=(const ConsoleCommand&) = delete;
			ConsoleCommand(ConsoleCommand&&) = delete;
			ConsoleCommand& operator=(ConsoleCommand&&) = delete;

			[[nodiscard]] std::string_view GetName() const { return m_Name; }

			[[nodiscard]] std::string_view GetHelp() const { return m_Help; }

			[[nodiscard]] ConsoleResult Invoke(const ConsoleArgs args) const { return m_Fn(args); }

			template <typename F>
			static void ForEach(F&& f) {
				for (const ConsoleCommand* command = s_Head.load(std::memory_order_acquire); command != nullptr; command = command->m_Next) {
					f(*command);
				}
			}

		private:
			std::string_view m_Name;
			std::string_view m_Help;
			Fn m_Fn{ nullptr };
			ConsoleCommand* m_Next{ nullptr };

			static inline constinit std::atomic<ConsoleCommand*> s_Head{ nullptr };
		};

		class Console {
		public:
			static void Init();

			static void Shutdown();

			[[nodiscard]] static bool GetStatus();

			[[nodiscard]] static ConsoleResult Execute(std::string_view line);

			static void Complete(std::string_view line, uint64_t caret, std::vector<ConsoleCompletion>& out, std::string_view& wordOut);

			[[nodiscard]] static std::string Usage(std::string_view name);

			[[nodiscard]] static std::string ContextUsage(std::string_view line, uint64_t caret);

			[[nodiscard]] static std::expected<void, CoriError<>> SaveArchive();

			[[nodiscard]] static std::expected<void, CoriError<>> LoadArchive();

			static void SetCheatsEnabled(bool enabled);

			[[nodiscard]] static bool GetCheatsEnabled();

			[[nodiscard]] static const CVarDesc* Find(std::string_view name);

			[[nodiscard]] static std::string FormatValue(const CVarDesc& desc);

			[[nodiscard]] static std::string FormatPendingValue(const CVarDesc& desc);

			[[nodiscard]] static std::string FormatDefault(const CVarDesc& desc);

			[[nodiscard]] static ConsoleResult SetValue(const CVarDesc& desc, std::string_view text);

			[[nodiscard]] static ConsoleResult ResetValue(const CVarDesc& desc);

			static void ForEachCVar(const std::function<void(const CVarDesc&)>& f);

			[[nodiscard]] static bool ConsumeClearRequest();

		private:
			static void AssertMainThread();
		};
	}
}

/**
 * @brief Defines a console command and registers it.
 * @details Body receives the arguments as `args` (a ConsoleArgs) and returns a ConsoleResult.
 */
#define CORI_CONSOLE_COMMAND(Identifier, Name, Help) \
	static ::Cori::Core::ConsoleResult CORI_CONCAT(ConsoleCommandFn_, Identifier)([[maybe_unused]] ::Cori::Core::ConsoleArgs args); \
	static const ::Cori::Core::ConsoleCommand CORI_CONCAT(s_ConsoleCommand_, Identifier){ Name, Help, &CORI_CONCAT(ConsoleCommandFn_, Identifier) }; \
	static ::Cori::Core::ConsoleResult CORI_CONCAT(ConsoleCommandFn_, Identifier)([[maybe_unused]] ::Cori::Core::ConsoleArgs args)
