#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/bundled/color.h>
#include <spdlog/fmt/bundled/base.h>
#include <spdlog/fmt/bundled/format.h>

namespace Cori {
	enum class LogLevel {
		TRACE,
		DEBUG,
		INFO,
		WARN,
		ERROR,
		FATAL
	};

	class Logger {
	public:
		static void Init(bool async);

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

		static void SetCoreLogLevel(LogLevel level);
		static void SetClientLogLevel(LogLevel level);

		template<typename T>
		static auto ColoredText(const T& text, fmt::color c, fmt::text_style s = fmt::text_style{}) {
			return fmt::styled(text, fmt::fg(c) | s);
		}

		template<typename T>
		static auto HighlitedText(const T& text, fmt::color c, fmt::text_style s = fmt::text_style{}) {
			return fmt::styled(text, fmt::bg(c) | s);
		}

		inline static std::string BoolAlpha(bool b) {
			if (b) {
				return "True";
			}
			else {
				return "False";
			}
		}

		static void EnableCoreTag(std::string_view tag);
		static void EnableCoreTags(std::initializer_list<const char*> tags);


		static void DisableCoreTag(std::string_view tag);
		static void DisableCoreTags(std::initializer_list<const char*> tags);

		static bool IsCoreTagEnabled(std::string_view tag);
		static void ClearCoreTagFilters();
		static std::vector<std::string> GetCoreActiveTags();


		static void EnableClientTag(std::string_view tag);
		static void EnableClientTags(std::initializer_list<const char*> tags);


		static void DisableClientTag(std::string_view tag);
		static void DisableClientTags(std::initializer_list<const char*> tags);

		static bool IsClientTagEnabled(std::string_view tag);
		static void ClearClientTagFilters();
		static std::vector<std::string> GetClientActiveTags();

		template<typename... Args>
		static void CoreLogTraceTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldCoreLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::thistle)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::light_cyan)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_CoreLogger->trace(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void CoreLogDebugTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldCoreLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::cyan)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::royal_blue)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_CoreLogger->debug(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void CoreLogInfoTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldCoreLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::lime)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::green_yellow)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_CoreLogger->info(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void CoreLogWarnTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldCoreLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::yellow)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::orange)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_CoreLogger->warn(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void CoreLogErrorTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldCoreLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::magenta)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::crimson)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_CoreLogger->error(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void CoreLogFatalTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldCoreLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::bg(fmt::color::red)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::bg(fmt::color::red)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_CoreLogger->critical(std::string_view(buffer.data(), buffer.size()));
			}
		}



		template<typename... Args>
		static void ClientLogTraceTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldClientLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::thistle)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::light_cyan)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_ClientLogger->trace(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void ClientLogDebugTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldClientLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::cyan)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::royal_blue)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_ClientLogger->debug(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void ClientLogInfoTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldClientLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::lime)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::green_yellow)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_ClientLogger->info(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void ClientLogWarnTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldClientLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::yellow)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::orange)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_ClientLogger->warn(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void ClientLogErrorTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldClientLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::fg(fmt::color::magenta)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::crimson)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_ClientLogger->error(std::string_view(buffer.data(), buffer.size()));
			}
		}

		template<typename... Args>
		static void ClientLogFatalTagged(std::initializer_list<const char*> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			if (ShouldClientLog(tags)) {

				fmt::memory_buffer buffer;

				for (const char* tag : tags) {
					fmt::format_to(std::back_inserter(buffer), "{}", fmt::styled(fmt::format("[{}]", tag), fmt::bg(fmt::color::red)));
				}

				buffer.push_back(' ');

				const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::bg(fmt::color::red)));
				const auto start_code_end = styled_dummy.find(' ');

				const auto end_code_start = start_code_end + 1;
				const std::string_view start_code(styled_dummy.data(), start_code_end);
				const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

				buffer.append(start_code);
				fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
				buffer.append(end_code);

				s_ClientLogger->critical(std::string_view(buffer.data(), buffer.size()));
			}
		}



		template<typename... Args>
		static void CoreLogTrace(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::light_cyan)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_CoreLogger->trace(std::string_view(buffer.data(), buffer.size()));

		}

		template<typename... Args>
		static void CoreLogDebug(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::royal_blue)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_CoreLogger->debug(std::string_view(buffer.data(), buffer.size()));

		}

		template<typename... Args>
		static void CoreLogInfo(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::green_yellow)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_CoreLogger->info(std::string_view(buffer.data(), buffer.size()));

		}

		template<typename... Args>
		static void CoreLogWarn(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::orange)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_CoreLogger->warn(std::string_view(buffer.data(), buffer.size()));
		}

		template<typename... Args>
		static void CoreLogError(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::crimson)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_CoreLogger->error(std::string_view(buffer.data(), buffer.size()));
		}

		template<typename... Args>
		static void CoreLogFatal(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::bg(fmt::color::red)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_CoreLogger->critical(std::string_view(buffer.data(), buffer.size()));
		}



		template<typename... Args>
		static void ClientLogTrace(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::light_cyan)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_ClientLogger->trace(std::string_view(buffer.data(), buffer.size()));
		}

		template<typename... Args>
		static void ClientLogDebug(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::royal_blue)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_ClientLogger->debug(std::string_view(buffer.data(), buffer.size()));
		}

		template<typename... Args>
		static void ClientLogInfo(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::green_yellow)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_ClientLogger->info(std::string_view(buffer.data(), buffer.size()));
		}

		template<typename... Args>
		static void ClientLogWarn(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::orange)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_ClientLogger->warn(std::string_view(buffer.data(), buffer.size()));
		}

		template<typename... Args>
		static void ClientLogError(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::fg(fmt::color::crimson)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_ClientLogger->error(std::string_view(buffer.data(), buffer.size()));
		}

		template<typename... Args>
		static void ClientLogFatal(const fmt::format_string<Args...>& fmt, Args&&... args) {
			fmt::memory_buffer buffer;

			const auto styled_dummy = fmt::format("{}", fmt::styled(" ", fmt::bg(fmt::color::red)));
			const auto start_code_end = styled_dummy.find(' ');

			const auto end_code_start = start_code_end + 1;
			const std::string_view start_code(styled_dummy.data(), start_code_end);
			const std::string_view end_code(styled_dummy.data() + end_code_start, styled_dummy.size() - end_code_start);

			buffer.append(start_code);
			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
			buffer.append(end_code);

			s_ClientLogger->critical(std::string_view(buffer.data(), buffer.size()));
		}


		static void SampleColors() {
			GetCoreLogger()->debug("Sample Text Start Here!!!!!!!!!!!!!!!!!!!!");
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: alice_blue", fmt::color::alice_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: antique_white", fmt::color::antique_white));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: aqua", fmt::color::aqua));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: aquamarine", fmt::color::aquamarine));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: azure", fmt::color::azure));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: beige", fmt::color::beige));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: bisque", fmt::color::bisque));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: black", fmt::color::black));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: blanched_almond", fmt::color::blanched_almond));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: blue", fmt::color::blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: blue_violet", fmt::color::blue_violet));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: brown", fmt::color::brown));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: burly_wood", fmt::color::burly_wood));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: cadet_blue", fmt::color::cadet_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: chartreuse", fmt::color::chartreuse));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: chocolate", fmt::color::chocolate));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: coral", fmt::color::coral));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: cornflower_blue", fmt::color::cornflower_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: cornsilk", fmt::color::cornsilk));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: crimson", fmt::color::crimson));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: cyan", fmt::color::cyan));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_blue", fmt::color::dark_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_cyan", fmt::color::dark_cyan));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_golden_rod", fmt::color::dark_golden_rod));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_gray", fmt::color::dark_gray));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_green", fmt::color::dark_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_khaki", fmt::color::dark_khaki));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_magenta", fmt::color::dark_magenta));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_olive_green", fmt::color::dark_olive_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_orange", fmt::color::dark_orange));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_orchid", fmt::color::dark_orchid));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_red", fmt::color::dark_red));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_salmon", fmt::color::dark_salmon));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_sea_green", fmt::color::dark_sea_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_slate_blue", fmt::color::dark_slate_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_slate_gray", fmt::color::dark_slate_gray));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_turquoise", fmt::color::dark_turquoise));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dark_violet", fmt::color::dark_violet));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: deep_pink", fmt::color::deep_pink));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: deep_sky_blue", fmt::color::deep_sky_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dim_gray", fmt::color::dim_gray));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: dodger_blue", fmt::color::dodger_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: fire_brick", fmt::color::fire_brick));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: floral_white", fmt::color::floral_white));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: forest_green", fmt::color::forest_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: fuchsia", fmt::color::fuchsia));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: gainsboro", fmt::color::gainsboro));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: ghost_white", fmt::color::ghost_white));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: gold", fmt::color::gold));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: golden_rod", fmt::color::golden_rod));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: gray", fmt::color::gray));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: green", fmt::color::green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: green_yellow", fmt::color::green_yellow));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: honey_dew", fmt::color::honey_dew));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: hot_pink", fmt::color::hot_pink));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: indian_red", fmt::color::indian_red));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: indigo", fmt::color::indigo));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: ivory", fmt::color::ivory));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: khaki", fmt::color::khaki));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: lavender", fmt::color::lavender));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: lavender_blush", fmt::color::lavender_blush));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: lawn_green", fmt::color::lawn_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: lemon_chiffon", fmt::color::lemon_chiffon));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_blue", fmt::color::light_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_coral", fmt::color::light_coral));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_cyan", fmt::color::light_cyan));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_golden_rod_yellow", fmt::color::light_golden_rod_yellow));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_gray", fmt::color::light_gray));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_green", fmt::color::light_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_pink", fmt::color::light_pink));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_salmon", fmt::color::light_salmon));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_sea_green", fmt::color::light_sea_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_sky_blue", fmt::color::light_sky_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_slate_gray", fmt::color::light_slate_gray));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_steel_blue", fmt::color::light_steel_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: light_yellow", fmt::color::light_yellow));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: lime", fmt::color::lime));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: lime_green", fmt::color::lime_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: linen", fmt::color::linen));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: magenta", fmt::color::magenta));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: maroon", fmt::color::maroon));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_aquamarine", fmt::color::medium_aquamarine));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_blue", fmt::color::medium_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_orchid", fmt::color::medium_orchid));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_purple", fmt::color::medium_purple));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_sea_green", fmt::color::medium_sea_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_slate_blue", fmt::color::medium_slate_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_spring_green", fmt::color::medium_spring_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_turquoise", fmt::color::medium_turquoise));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: medium_violet_red", fmt::color::medium_violet_red));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: midnight_blue", fmt::color::midnight_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: mint_cream", fmt::color::mint_cream));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: misty_rose", fmt::color::misty_rose));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: moccasin", fmt::color::moccasin));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: navajo_white", fmt::color::navajo_white));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: navy", fmt::color::navy));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: old_lace", fmt::color::old_lace));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: olive", fmt::color::olive));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: olive_drab", fmt::color::olive_drab));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: orange", fmt::color::orange));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: orange_red", fmt::color::orange_red));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: orchid", fmt::color::orchid));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: pale_golden_rod", fmt::color::pale_golden_rod));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: pale_green", fmt::color::pale_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: pale_turquoise", fmt::color::pale_turquoise));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: pale_violet_red", fmt::color::pale_violet_red));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: papaya_whip", fmt::color::papaya_whip));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: peach_puff", fmt::color::peach_puff));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: peru", fmt::color::peru));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: pink", fmt::color::pink));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: plum", fmt::color::plum));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: powder_blue", fmt::color::powder_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: purple", fmt::color::purple));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: rebecca_purple", fmt::color::rebecca_purple));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: red", fmt::color::red));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: rosy_brown", fmt::color::rosy_brown));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: royal_blue", fmt::color::royal_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: saddle_brown", fmt::color::saddle_brown));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: salmon", fmt::color::salmon));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: sandy_brown", fmt::color::sandy_brown));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: sea_green", fmt::color::sea_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: sea_shell", fmt::color::sea_shell));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: sienna", fmt::color::sienna));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: silver", fmt::color::silver));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: sky_blue", fmt::color::sky_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: slate_blue", fmt::color::slate_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: slate_gray", fmt::color::slate_gray));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: snow", fmt::color::snow));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: spring_green", fmt::color::spring_green));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: steel_blue", fmt::color::steel_blue));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: tan", fmt::color::tan));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: teal", fmt::color::teal));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: thistle", fmt::color::thistle));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: tomato", fmt::color::tomato));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: turquoise", fmt::color::turquoise));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: violet", fmt::color::violet));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: wheat", fmt::color::wheat));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: white", fmt::color::white));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: white_smoke", fmt::color::white_smoke));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: yellow", fmt::color::yellow));
			GetCoreLogger()->debug("{}", ColoredText("Sample Text color: yellow_green", fmt::color::yellow_green));
		}

	private:
		static bool ShouldCoreLog(std::initializer_list<const char*> tags);
		static bool ShouldClientLog(std::initializer_list<const char*> tags);

		struct StringHash {
			using is_transparent = void;
			[[nodiscard]] size_t operator()(const char* txt) const {
				return std::hash<std::string_view>{}(txt);
			}
			[[nodiscard]] size_t operator()(std::string_view txt) const {
				return std::hash<std::string_view>{}(txt);
			}
			[[nodiscard]] size_t operator()(const std::string& txt) const {
				return std::hash<std::string_view>{}(txt);
			}
		};

		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
		static std::unordered_set<std::string, StringHash, std::equal_to<>> s_CoreActiveTags;
		static std::unordered_set<std::string, StringHash, std::equal_to<>> s_ClientActiveTags;
	};
}

const std::string CORI_SECOND_LINE_SPACING = "[" + std::string(43, '-') + "]: ";

#ifdef DEBUG_BUILD

#define CORI_CORE_TRACE(...) ::Cori::Logger::CoreLogTrace(__VA_ARGS__)
#define CORI_CORE_DEBUG(...) ::Cori::Logger::CoreLogDebug(__VA_ARGS__)
#define CORI_CORE_INFO(...)  ::Cori::Logger::CoreLogInfo(__VA_ARGS__)

#else

#define CORI_CORE_TRACE(...)
#define CORI_CORE_DEBUG(...)
#define CORI_CORE_INFO(...)

#endif

#define CORI_CORE_WARN(...)  ::Cori::Logger::CoreLogWarn(__VA_ARGS__)
#define CORI_CORE_ERROR(...) ::Cori::Logger::CoreLogWarn(__VA_ARGS__)
#define CORI_CORE_FATAL(...) ::Cori::Logger::CoreLogWarn(__VA_ARGS__)

#define CORI_TRACE(...)      ::Cori::Logger::ClientLogTrace(__VA_ARGS__)
#define CORI_DEBUG(...)      ::Cori::Logger::ClientLogDebug(__VA_ARGS__)
#define CORI_INFO(...)       ::Cori::Logger::ClientLogInfo(__VA_ARGS__)
#define CORI_WARN(...)       ::Cori::Logger::ClientLogWarn(__VA_ARGS__)
#define CORI_ERROR(...)      ::Cori::Logger::ClientLogError(__VA_ARGS__)
#define CORI_FATAL(...)      ::Cori::Logger::ClientLogFatal(__VA_ARGS__)

#ifdef DEBUG_BUILD

#define CORI_CORE_ASSERT_DEBUG(x, ...) (!(x) ? (CORI_CORE_DEBUG("Assertion Failed, message: " __VA_ARGS__), CORI_CORE_DEBUG("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_ASSERT_INFO(x, ...)  (!(x) ? (CORI_CORE_INFO("Assertion Failed, message: " __VA_ARGS__), CORI_CORE_INFO("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_ASSERT_WARN(x, ...)  (!(x) ? (CORI_CORE_WARN("Assertion Failed, message: " __VA_ARGS__), CORI_CORE_WARN("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_ASSERT_ERROR(x, ...) (!(x) ? (CORI_CORE_ERROR("Assertion Failed, message: " __VA_ARGS__), CORI_CORE_ERROR("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_ASSERT_FATAL(x, ...) (!(x) ? (CORI_CORE_FATAL("Assertion Failed, message: " __VA_ARGS__), CORI_CORE_FATAL("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), __builtin_debugtrap(), true) : false)

#define CORI_CORE_VERIFY_DEBUG(x, ...) (!(x) ? (CORI_CORE_DEBUG("Verify Failed, message: " __VA_ARGS__), CORI_CORE_DEBUG("    Details - (Verify: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_VERIFY_INFO(x, ...)  (!(x) ? (CORI_CORE_INFO("Verify Failed, message: " __VA_ARGS__), CORI_CORE_INFO("    Details - (Verify: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_VERIFY_WARN(x, ...)  (!(x) ? (CORI_CORE_WARN("Verify Failed, message: " __VA_ARGS__), CORI_CORE_WARN("    Details - (Verify: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_VERIFY_ERROR(x, ...) (!(x) ? (CORI_CORE_ERROR("Verify Failed, message: " __VA_ARGS__), CORI_CORE_ERROR("    Details - (Verify: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_CORE_VERIFY_FATAL(x, ...) (!(x) ? (CORI_CORE_FATAL("Verify Failed, message: " __VA_ARGS__), CORI_CORE_FATAL("    Details - (Verify: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), __builtin_debugtrap(), true) : false)

#else

#define CORI_CORE_ASSERT_DEBUG(x, ...) false
#define CORI_CORE_ASSERT_INFO(x, ...) false
#define CORI_CORE_ASSERT_WARN(x, ...) false
#define CORI_CORE_ASSERT_ERROR(x, ...) false
#define CORI_CORE_ASSERT_FATAL(x, ...) false

#define CORI_CORE_VERIFY_DEBUG(x, ...) (x && false)
#define CORI_CORE_VERIFY_INFO(x, ...)  (x && false)
#define CORI_CORE_VERIFY_WARN(x, ...)  (x && false)
#define CORI_CORE_VERIFY_ERROR(x, ...) (x && false)
#define CORI_CORE_VERIFY_FATAL(x, ...) (x && false)

#endif

#define CORI_ASSERT_DEBUG(x, ...) (!(x) ? (CORI_DEBUG("Assertion Failed, message: " __VA_ARGS__), CORI_DEBUG("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_ASSERT_INFO(x, ...)  (!(x) ? (CORI_INFO("Assertion Failed, message: " __VA_ARGS__), CORI_INFO("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_ASSERT_WARN(x, ...)  (!(x) ? (CORI_WARN("Assertion Failed, message: " __VA_ARGS__), CORI_WARN("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_ASSERT_ERROR(x, ...) (!(x) ? (CORI_ERROR("Assertion Failed, message: " __VA_ARGS__), CORI_ERROR("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), true) : false)
#define CORI_ASSERT_FATAL(x, ...) (!(x) ? (CORI_FATAL("Assertion Failed, message: " __VA_ARGS__), CORI_FATAL("    Details - (Assert: '{}', Function: '{}', Line: '{}')", #x, __PRETTY_FUNCTION__, __LINE__), __builtin_debugtrap(), true) : false)

#define CORI_CORE_TRACE_TAGGED(...) ::Cori::Logger::CoreLogTraceTagged(__VA_ARGS__)
#define CORI_CORE_DEBUG_TAGGED(...) ::Cori::Logger::CoreLogDebugTagged(__VA_ARGS__)
#define CORI_CORE_INFO_TAGGED(...)  ::Cori::Logger::CoreLogInfoTagged(__VA_ARGS__)
#define CORI_CORE_WARN_TAGGED(...)  ::Cori::Logger::CoreLogWarnTagged(__VA_ARGS__)
#define CORI_CORE_ERROR_TAGGED(...) ::Cori::Logger::CoreLogErrorTagged(__VA_ARGS__)
#define CORI_CORE_FATAL_TAGGED(...) ::Cori::Logger::CoreLogFatalTagged(__VA_ARGS__)

#define CORI_TRACE_TAGGED(...) ::Cori::Logger::ClientLogTraceTagged(__VA_ARGS__)
#define CORI_DEBUG_TAGGED(...) ::Cori::Logger::ClientLogDebugTagged(__VA_ARGS__)
#define CORI_INFO_TAGGED(...)  ::Cori::Logger::ClientLogInfoTagged(__VA_ARGS__)
#define CORI_WARN_TAGGED(...)  ::Cori::Logger::ClientLogWarnTagged(__VA_ARGS__)
#define CORI_ERROR_TAGGED(...) ::Cori::Logger::ClientLogErrorTagged(__VA_ARGS__)
#define CORI_FATAL_TAGGED(...) ::Cori::Logger::ClientLogFatalTagged(__VA_ARGS__)