#include "Logger.hpp"
#include "LogBuffer.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/details/os.h>
#include <shared_mutex>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace {
	std::shared_mutex s_ThreadNameMutex;
	std::unordered_map<uint64_t, std::string> s_ThreadNames;

	constexpr std::string_view s_ResetCode{ "\x1b[0m" };

	[[nodiscard]] std::string ForegroundCode(const fmt::color color) {
		const auto rgb = static_cast<uint32_t>(color);
		return fmt::format("\x1b[38;2;{};{};{}m", (rgb >> 16) & 0xFFu, (rgb >> 8) & 0xFFu, rgb & 0xFFu);
	}

	[[nodiscard]] std::string BackgroundCode(const fmt::color color) {
		const auto rgb = static_cast<uint32_t>(color);
		return fmt::format("\x1b[48;2;{};{};{}m", (rgb >> 16) & 0xFFu, (rgb >> 8) & 0xFFu, rgb & 0xFFu);
	}

	struct LevelPalette {
		std::string m_TagCode;
		std::string m_MessageCode;
	};

	const std::array<LevelPalette, 6>& GetLevelPalettes() {
		static const std::array palettes{
			LevelPalette{ ForegroundCode(fmt::color::thistle),  ForegroundCode(fmt::color::light_cyan) },
			LevelPalette{ ForegroundCode(fmt::color::cyan),     ForegroundCode(fmt::color::royal_blue) },
			LevelPalette{ ForegroundCode(fmt::color::lime),     ForegroundCode(fmt::color::green_yellow) },
			LevelPalette{ ForegroundCode(fmt::color::yellow),   ForegroundCode(fmt::color::orange) },
			LevelPalette{ ForegroundCode(fmt::color::hot_pink), ForegroundCode(fmt::color::violet) },
			LevelPalette{ BackgroundCode(fmt::color::red),      BackgroundCode(fmt::color::red) }
		};
		return palettes;
	}
}

namespace Cori {
	class ThreadNameFlagFormatter final : public spdlog::custom_flag_formatter {
	public:
		void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
			const std::string label = Logger::GetThreadName(msg.thread_id);

			const std::string_view text = label;
			const auto append = [&dest](const std::string_view s) {
				dest.append(s.data(), s.data() + s.size());
			};

			const auto appendSpaces = [&dest](const uint64_t n) {
				const std::string sp(n, ' '); dest.append(sp.data(), sp.data() + sp.size());
			};

			if (padinfo_.enabled() && text.size() < padinfo_.width_) {
				const uint64_t pad = padinfo_.width_ - text.size();
				using pad_side = spdlog::details::padding_info::pad_side;
				switch (padinfo_.side_) {
				case pad_side::left:
					appendSpaces(pad);
					append(text);
					break;
				case pad_side::center:
					appendSpaces(pad / 2);
					append(text);
					appendSpaces(pad - pad / 2);
					break;
				case pad_side::right:
				default:
					append(text);
					appendSpaces(pad);
					break;
				}
			}
			else {
				append(text);
			}
		}

		[[nodiscard]] std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
			return spdlog::details::make_unique<ThreadNameFlagFormatter>();
		}
	};

	class ColorizedPayloadFlagFormatter final : public spdlog::custom_flag_formatter {
	public:
		void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
			const std::string_view payload(msg.payload.data(), msg.payload.size());

			const auto append = [&dest](const std::string_view s) {
				dest.append(s.data(), s.data() + s.size());
			};

			const auto levelIndex = static_cast<uint64_t>(msg.level);
			if (levelIndex >= GetLevelPalettes().size()) {
				append(payload);
				return;
			}

			const LevelPalette& palette = GetLevelPalettes()[levelIndex];

			const auto [tags, message] = Cori::SplitTagPrefix(payload);

			if (!tags.empty()) {
				append(palette.m_TagCode);
				append(tags);
				append(s_ResetCode);
			}

			if (!message.empty()) {
				append(palette.m_MessageCode);
				append(message);
				append(s_ResetCode);
			}
		}

		[[nodiscard]] std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
			return spdlog::details::make_unique<ColorizedPayloadFlagFormatter>();
		}
	};



	std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Logger::s_ClientLogger;

	bool Logger::s_Initialized = false;

	void Logger::EnableVirtualTerminalProcessing() {
#ifdef PLATFORM_WINDOWS
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE) {
			return;
		}

		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode)) {
			return;
		}

		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

		if (!SetConsoleMode(hOut, dwMode)) {
			return;
		}
#endif
	}

	void Logger::Init(const bool async, const  bool fileWrite) {
		EnableVirtualTerminalProcessing();
		if (async) {
			spdlog::init_thread_pool(8192, 1);
		}

		int32_t maxSize = 1048576 * 20;
		int32_t maxFiles = 5;

		constexpr const char* filePattern = "[%Y-%m-%d %H:%M:%S.%e] [%-10N] [%-6n] [%-8l]: %v %@";

		const auto makeFileFormatter = [filePattern] {
			auto formatter = std::make_unique<spdlog::pattern_formatter>();
			formatter->add_flag<ThreadNameFlagFormatter>('N').set_pattern(filePattern);
			return formatter;
		};

		const auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/cori_log.log", maxSize, maxFiles);
		fileSink->set_formatter(makeFileFormatter());
		std::vector<spdlog::sink_ptr> coreSinks;
		std::vector<spdlog::sink_ptr> clientSinks;

		if (fileWrite) {
			coreSinks.push_back(fileSink);

			clientSinks.push_back(fileSink);
		}

		const spdlog::sink_ptr bufferSink = LogBuffer::MakeSink();
		coreSinks.push_back(bufferSink);
		clientSinks.push_back(bufferSink);

#ifdef DEBUG_BUILD
		constexpr const char* consolePattern = "[%Y-%m-%d %H:%M:%S.%e] [%-10N] [%-6n] %^[%-8l]%$: %v %@";

		const bool colorTerminal = spdlog::details::os::is_color_terminal() && spdlog::details::os::in_terminal(stdout);

		const auto makeConsoleFormatter = [consolePattern, colorTerminal] {
			auto formatter = std::make_unique<spdlog::pattern_formatter>();
			formatter->add_flag<ThreadNameFlagFormatter>('N');
			if (colorTerminal) {
				formatter->add_flag<ColorizedPayloadFlagFormatter>('v');
			}
			formatter->set_pattern(consolePattern);
			return formatter;
		};

		const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		consoleSink->set_formatter(makeConsoleFormatter());

		coreSinks.push_back(consoleSink);
		clientSinks.push_back(consoleSink);
#endif

		if (async) {
			s_CoreLogger = std::make_shared<spdlog::async_logger>("ENGINE", coreSinks.begin(), coreSinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
			s_ClientLogger = std::make_shared<spdlog::async_logger>("APP", clientSinks.begin(), clientSinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		}
		else {
			s_CoreLogger = std::make_shared<spdlog::logger>("ENGINE", coreSinks.begin(), coreSinks.end());
			s_ClientLogger = std::make_shared<spdlog::logger>("APP", clientSinks.begin(), clientSinks.end());
		}

		spdlog::register_logger(s_CoreLogger);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::warn);

		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::warn);

		s_Initialized = true;

		Cori::SetThreadName("Main");

		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "------------- NEW LOG SESSION -------------");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "|  Logger initialized. Mode: {} |", async ? "Asynchronous" : "Synchronous ");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "-------------------------------------------");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "|     File logging is: {}           |", fileWrite ? "Enabled " : "Disabled");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "-------------------------------------------");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "|     Registered log tags: {:<3}            |", LogTag::Count());
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "-------------------------------------------");
	}

	void Logger::SetThreadName(const std::string& name) {
		const uint64_t id = spdlog::details::os::thread_id();
		std::unique_lock lock(s_ThreadNameMutex);
		s_ThreadNames[id] = name;
	}

	std::string Logger::GetThreadName(const uint64_t threadId) {
		{
			std::shared_lock lock(s_ThreadNameMutex);
			if (const auto it = s_ThreadNames.find(threadId); it != s_ThreadNames.end()) {
				return it->second;
			}
		}
		return std::to_string(threadId);
	}

	bool Logger::GetStatus() {
		return s_Initialized;
	}

	void Logger::SetClientLogLevel(const LogLevel level) {
		if (!s_ClientLogger) {
			return;
		}
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Client log level is set to {}", LogLevelName(level));
		s_ClientLogger->set_level(ToSpdlogLevel(level));
	}

	void Logger::SetCoreLogLevel(const LogLevel level) {
		if (!s_CoreLogger) {
			return;
		}
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Core log level is set to {}", LogLevelName(level));
		s_CoreLogger->set_level(ToSpdlogLevel(level));
	}



	void Logger::SampleColors() {
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

}
