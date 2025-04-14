
#include "Logger.hpp"

namespace Cori {

	std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Logger::s_ClientLogger;

	void Logger::Init() {
		spdlog::set_pattern("%^[%T] %n-%l : %v%$");
		s_CoreLogger = spdlog::stdout_color_mt("ENGINE"); 
		s_CoreLogger->set_level(spdlog::level::trace);
		s_ClientLogger = spdlog::stdout_color_mt("APP");
		s_ClientLogger->set_level(spdlog::level::trace);

		CORI_CORE_INFO("Logger initialized");
	}
}
