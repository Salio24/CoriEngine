#include "Logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace {
	std::mutex s_CoreTagMutex;
	std::mutex s_ClientTagMutex;
}

namespace Cori {

	std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Logger::s_ClientLogger;

	std::unordered_set<std::string, Logger::StringHash, std::equal_to<>> Logger::s_CoreInactiveTags;
	std::unordered_set<std::string, Logger::StringHash, std::equal_to<>> Logger::s_ClientInactiveTags;

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
		if (async) {
			spdlog::init_thread_pool(8192, 1);
		}

		int32_t maxSize = 1048576 * 20;
		int32_t maxFiles = 5;
		const auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/cori_log.txt", maxSize, maxFiles);
		fileSink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [Thread %t] [%-6n] [%-8l]: %v%$ %@");
		std::vector<spdlog::sink_ptr> coreSinks;
		std::vector<spdlog::sink_ptr> clientSinks;

		if (fileWrite) {
			coreSinks.push_back(fileSink);

			clientSinks.push_back(fileSink);
		}

#ifdef DEBUG_BUILD
		const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		consoleSink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [Thread %t] [%-6n] [%-8l]: %v%$ %@");

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
		//s_CoreLogger->flush_on(spdlog::level::trace);

		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::warn);
		//s_ClientLogger->flush_on(spdlog::level::trace);

		s_Initialized = true;

		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "------------- NEW LOG SESSION -------------");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "|  Logger initialized. Mode: {} |", async ? "Asynchronous" : "Synchronous ");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "-------------------------------------------");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "|     File logging is: {}           |", fileWrite ? "Enabled " : "Disabled");
		CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "-------------------------------------------");
	}

	bool Logger::GetStatus() {
		return s_Initialized;
	}

	void Logger::SetClientLogLevel(const LogLevel level) {
		switch (level) {
		case LogLevel::CORI_TRACE:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Client log level is set to TRACE");
			s_ClientLogger->set_level(spdlog::level::trace);
			break;
		case LogLevel::CORI_DEBUG:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Client log level is set to DEBUG");
			s_ClientLogger->set_level(spdlog::level::debug);
			break;
		case LogLevel::CORI_INFO:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Client log level is set to INFO");
			s_ClientLogger->set_level(spdlog::level::info);
			break;
		case LogLevel::CORI_WARN:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Client log level is set to WARN");
			s_ClientLogger->set_level(spdlog::level::warn);
			break;
		case LogLevel::CORI_ERROR:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Client log level is set to ERROR");
			s_ClientLogger->set_level(spdlog::level::err);
			break;
		case LogLevel::CORI_FATAL:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Client log level is set to FATAL");
			s_ClientLogger->set_level(spdlog::level::critical);
			break;
		}
	}

	void Logger::SetCoreLogLevel(const LogLevel level) {
		switch (level) {
		case LogLevel::CORI_TRACE:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Core log level is set to TRACE");
			s_CoreLogger->set_level(spdlog::level::trace);
			break;
		case LogLevel::CORI_DEBUG:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Core log level is set to DEBUG");
			s_CoreLogger->set_level(spdlog::level::debug);
			break;
		case LogLevel::CORI_INFO:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Core log level is set to INFO");
			s_CoreLogger->set_level(spdlog::level::info);
			break;
		case LogLevel::CORI_WARN:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Core log level is set to WARN");
			s_CoreLogger->set_level(spdlog::level::warn);
			break;
		case LogLevel::CORI_ERROR:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Core log level is set to ERROR");
			s_CoreLogger->set_level(spdlog::level::err);
			break;
		case LogLevel::CORI_FATAL:
			CORI_CORE_INFO_TAGGED({ Tags::Core::Self, Tags::Core::Logger }, "Core log level is set to FATAL");
			s_CoreLogger->set_level(spdlog::level::critical);
			break;
		}
	}

	void Logger::EnableCoreTag(const char* tag) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		s_CoreInactiveTags.erase(std::string(tag));
	}

	void Logger::EnableCoreTags(const std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		for (const char* tag : tags) {
			s_CoreInactiveTags.erase(tag);
		}
	}

	void Logger::DisableCoreTag(const char* tag) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		s_CoreInactiveTags.insert(std::string(tag));
	}

	void Logger::DisableCoreTags(const std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		for (const char* tag : tags) {
			s_CoreInactiveTags.insert(tag);
		}
	}

	bool Logger::IsCoreTagDisabled(const char* tag) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		return s_CoreInactiveTags.contains(tag);
	}

	void Logger::ClearCoreTagFilter() {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		s_CoreInactiveTags.clear();
	}


	std::vector<std::string> Logger::GetCoreInactiveTags() {
		std::vector<std::string> result;
		result.reserve(s_CoreInactiveTags.size());
		result.insert(result.end(), s_CoreInactiveTags.begin(), s_CoreInactiveTags.end());
		return result;
	}

	void Logger::EnableClientTag(const char* tag) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		s_ClientInactiveTags.erase(std::string(tag));
	}

	void Logger::EnableClientTags(const std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		for (const char* tag : tags) {
			s_ClientInactiveTags.erase(tag);
		}
	}

	void Logger::DisableClientTag(const char* tag) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		s_ClientInactiveTags.insert(std::string(tag));
	}

	void Logger::DisableClientTags(const std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		if (!s_ClientInactiveTags.empty()) {
			for (const char* tag : tags) {
				s_ClientInactiveTags.insert(tag);
			}
		}
	}

	bool Logger::IsClientTagDisabled(const char* tag) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		return s_ClientInactiveTags.contains(tag);
	}

	void Logger::ClearClientTagFilter() {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		s_ClientInactiveTags.clear();
	}


	std::vector<std::string> Logger::GetClientInactiveTags() {
		std::vector<std::string> result;
		result.reserve(s_ClientInactiveTags.size());
		result.insert(result.end(), s_ClientInactiveTags.begin(), s_ClientInactiveTags.end());
		return result;
	}

	bool Logger::ShouldCoreLog(const std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		if (s_CoreInactiveTags.empty()) {
			return true;
		}
		for (const char* tag : tags) {
			if (s_CoreInactiveTags.contains(tag)) {
				return false;
			}
		}
		return true;
	}

	bool Logger::ShouldClientLog(const std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		if (s_ClientInactiveTags.empty()) {
			return true;
		}
		for (const char* tag : tags) {
			if (s_ClientInactiveTags.contains(tag)) {
				return false;
			}
		}
		return true;
	}

}
