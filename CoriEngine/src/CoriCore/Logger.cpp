
#include "Logger.hpp"

namespace Cori {

	std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Logger::s_ClientLogger;

	void Logger::Init() {
		spdlog::set_pattern("%^[%T] %n: %v%$");
		s_CoreLogger = spdlog::stdout_color_mt("ENGINE"); 
		s_CoreLogger->set_level(spdlog::level::trace);
		s_ClientLogger = spdlog::stdout_color_mt("APP");
		s_ClientLogger->set_level(spdlog::level::trace);

		CORI_CORE_INFO("Logger initialized");
		CORI_CORE_TRACE("THIS IS TRACE TEST");
		CORI_CORE_DEBUG("THIS IS DEBUG TEST");
		CORI_CORE_INFO("THIS IS INFO TEST");
		CORI_CORE_WARN("THIS IS WARN TEST");
		CORI_CORE_ERROR("THIS IS ERROR TEST");
		CORI_CORE_FATAL("THIS IS FATAL TEST");
	}
}
