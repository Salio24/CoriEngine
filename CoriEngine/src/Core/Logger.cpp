// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Logger.hpp"

namespace Cori {

	std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Logger::s_ClientLogger;

	void Logger::Init() {
		std::vector<spdlog::sink_ptr> core_sinks;
		std::vector<spdlog::sink_ptr> client_sinks;

		int max_size = 1048576 * 20; // 20 MB
		int max_files = 5;

		auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/cori_log.txt", max_size, max_files);
		file_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%-6n] [%-8l]: %v%$");

		core_sinks.push_back(file_sink);
		client_sinks.push_back(file_sink);

#ifdef DEBUG_BUILD
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%-6n] [%-8l]: %v%$");

		core_sinks.push_back(console_sink);
		client_sinks.push_back(console_sink);
#endif
		s_CoreLogger = std::make_shared<spdlog::logger>("ENGINE", core_sinks.begin(), core_sinks.end());
		spdlog::register_logger(s_CoreLogger);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::warn);

		s_ClientLogger = std::make_shared<spdlog::logger>("APP", client_sinks.begin(), client_sinks.end());
		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::warn);

		std::string aa = CORI_SECOND_LINE_SPACING;

		CORI_CORE_INFO("");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("Logger initialized");
		CORI_CORE_INFO("ATTENTION! NEW LOG FROM NEW SESSION STARTS HERE!");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("");

	}

	void Logger::SetClientLogLevel(LogLevel level) {
		switch (level) {
		case Cori::LogLevel::TRACE:
			CORI_CORE_INFO("Client log level is set to TRACE");
			s_ClientLogger->set_level(spdlog::level::trace);
			break;
		case Cori::LogLevel::DEBUG:
			CORI_CORE_INFO("Client log level is set to DEBUG");
			s_ClientLogger->set_level(spdlog::level::debug);
			break;
		case Cori::LogLevel::INFO:
			CORI_CORE_INFO("Client log level is set to INFO");
			s_ClientLogger->set_level(spdlog::level::info);
			break;
		case Cori::LogLevel::WARN:
			CORI_CORE_INFO("Client log level is set to WARN");
			s_ClientLogger->set_level(spdlog::level::warn);
			break;
		case Cori::LogLevel::ERROR:
			CORI_CORE_INFO("Client log level is set to ERROR");
			s_ClientLogger->set_level(spdlog::level::err);
			break;
		case Cori::LogLevel::FATAL:
			CORI_CORE_INFO("Client log level is set to FATAL");
			s_ClientLogger->set_level(spdlog::level::critical);
			break;
		}
	}

	void Logger::SetCoreLogLevel(LogLevel level) {
		switch (level) {
		case Cori::LogLevel::TRACE:
			CORI_CORE_INFO("Core log level is set to TRACE");
			s_CoreLogger->set_level(spdlog::level::trace);
			break;
		case Cori::LogLevel::DEBUG:
			CORI_CORE_INFO("Core log level is set to DEBUG");
			s_CoreLogger->set_level(spdlog::level::debug);
			break;
		case Cori::LogLevel::INFO:
			CORI_CORE_INFO("Core log level is set to INFO");
			s_CoreLogger->set_level(spdlog::level::info);
			break;
		case Cori::LogLevel::WARN:
			CORI_CORE_INFO("Core log level is set to WARN");
			s_CoreLogger->set_level(spdlog::level::warn);
			break;
		case Cori::LogLevel::ERROR:
			CORI_CORE_INFO("Core log level is set to ERROR");
			s_CoreLogger->set_level(spdlog::level::err);
			break;
		case Cori::LogLevel::FATAL:
			CORI_CORE_INFO("Core log level is set to FATAL");
			s_CoreLogger->set_level(spdlog::level::critical);
			break;
		}
	}
}
