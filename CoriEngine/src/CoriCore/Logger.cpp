// This is a personal academic project. Dear PVS-Studio, please check it.
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
		file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

		core_sinks.push_back(file_sink);
		client_sinks.push_back(file_sink);

#ifdef DEBUG_BUILD
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_pattern("%^[%T] %n-%l: %v%$");

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

		CORI_CORE_INFO("");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("Logger initialized");
		CORI_CORE_INFO("ATTENTION! NEW LOG FROM NEW SESSION STARTS HERE!");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("");
		CORI_CORE_INFO("");
	}
}
