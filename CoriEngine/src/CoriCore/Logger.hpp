#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>

namespace Cori {
	class Logger {
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

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
