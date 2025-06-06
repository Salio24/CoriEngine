#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/bundled/color.h>

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
		static void Init();

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
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

const std::string CORI_SECOND_LINE_SPACING = "[" + std::string(43, '-') + "]: ";

#define CORI_CORE_TRACE(...) ::Cori::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define CORI_CORE_DEBUG(...) ::Cori::Logger::GetCoreLogger()->debug(__VA_ARGS__)
#define CORI_CORE_INFO(...)  ::Cori::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define CORI_CORE_WARN(...)  ::Cori::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define CORI_CORE_ERROR(...) ::Cori::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define CORI_CORE_FATAL(...) ::Cori::Logger::GetCoreLogger()->critical(__VA_ARGS__)

#define CORI_TRACE(...)      ::Cori::Logger::GetClientLogger()->trace(__VA_ARGS__)
#define CORI_DEBUG(...)      ::Cori::Logger::GetClientLogger()->debug(__VA_ARGS__)
#define CORI_INFO(...)       ::Cori::Logger::GetClientLogger()->info(__VA_ARGS__)
#define CORI_WARN(...)       ::Cori::Logger::GetClientLogger()->warn(__VA_ARGS__)
#define CORI_ERROR(...)      ::Cori::Logger::GetClientLogger()->error(__VA_ARGS__)
#define CORI_FATAL(...)      ::Cori::Logger::GetClientLogger()->critical(__VA_ARGS__)

// TODO: utilize pretty_function in asserts and verfies

#define CORI_CORE_ASSERT_DEBUG(x, ...) (!(x) ? (CORI_CORE_DEBUG("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_ASSERT_INFO(x, ...)  (!(x) ? (CORI_CORE_INFO("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_ASSERT_WARN(x, ...)  (!(x) ? (CORI_CORE_WARN("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_ASSERT_ERROR(x, ...) (!(x) ? (CORI_CORE_ERROR("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_ASSERT_FATAL(x, ...) (!(x) ? (CORI_CORE_FATAL("Assertion Failed: " __VA_ARGS__), __builtin_debugtrap(), true) : false)

#define CORI_CORE_VERIFY_DEBUG(x, ...) (!(x) ? (CORI_CORE_DEBUG("Verify Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_VERIFY_INFO(x, ...)  (!(x) ? (CORI_CORE_INFO("Verify Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_VERIFY_WARN(x, ...)  (!(x) ? (CORI_CORE_WARN("Verify Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_VERIFY_ERROR(x, ...) (!(x) ? (CORI_CORE_ERROR("Verify Failed: " __VA_ARGS__), true) : false)
#define CORI_CORE_VERIFY_FATAL(x, ...) (!(x) ? (CORI_CORE_FATAL("Verify Failed: " __VA_ARGS__), __builtin_debugtrap(), true) : false)

#define CORI_ASSERT_DEBUG(x, ...) (!(x) ? (CORI_DEBUG("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_ASSERT_INFO(x, ...)  (!(x) ? (CORI_INFO("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_ASSERT_WARN(x, ...)  (!(x) ? (CORI_WARN("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_ASSERT_ERROR(x, ...) (!(x) ? (CORI_ERROR("Assertion Failed: " __VA_ARGS__), true) : false)
#define CORI_ASSERT_FATAL(x, ...) (!(x) ? (CORI_FATAL("Assertion Failed: " __VA_ARGS__), __builtin_debugtrap(), true) : false)