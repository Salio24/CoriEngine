// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Logger.hpp"

namespace Cori {

	std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Logger::s_ClientLogger;

	std::unordered_set<std::string, Logger::StringHash, std::equal_to<>> Logger::s_CoreActiveTags;
	std::unordered_set<std::string, Logger::StringHash, std::equal_to<>> Logger::s_ClientActiveTags;

	static std::mutex s_CoreTagMutex;
	static std::mutex s_ClientTagMutex;

	void Logger::Init(bool async) {
		if (async) {
			spdlog::init_thread_pool(8192, 1);
		}

		int max_size = 1048576 * 20;
		int max_files = 5;
		auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/cori_log.txt", max_size, max_files);
		file_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%-6n] [%-8l]: %v%$");

		std::vector<spdlog::sink_ptr> core_sinks;
		core_sinks.push_back(file_sink);

		std::vector<spdlog::sink_ptr> client_sinks;
		client_sinks.push_back(file_sink);

#ifdef DEBUG_BUILD
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%-6n] [%-8l]: %v%$");

		core_sinks.push_back(console_sink);
		client_sinks.push_back(console_sink);
#endif

		if (async) {
			s_CoreLogger = std::make_shared<spdlog::async_logger>("ENGINE", core_sinks.begin(), core_sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
			s_ClientLogger = std::make_shared<spdlog::async_logger>("APP", client_sinks.begin(), client_sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		}
		else {
			s_CoreLogger = std::make_shared<spdlog::logger>("ENGINE", core_sinks.begin(), core_sinks.end());
			s_ClientLogger = std::make_shared<spdlog::logger>("APP", client_sinks.begin(), client_sinks.end());
		}

		spdlog::register_logger(s_CoreLogger);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::warn);

		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::warn);


		CORI_CORE_INFO("------------- NEW LOG SESSION -------------");
		CORI_CORE_INFO("|  Logger initialized. Mode: {} |", async ? "Asynchronous" : "Synchronous ");
		CORI_CORE_INFO("-------------------------------------------");
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

	void Logger::EnableCoreTag(std::string_view tag) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		s_CoreActiveTags.insert(std::string(tag));
	}

	void Logger::EnableCoreTags(std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		for (const char* tag : tags) {
			s_CoreActiveTags.insert(tag);
		}
	}

	void Logger::DisableCoreTag(std::string_view tag) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		s_CoreActiveTags.erase(std::string(tag));
	}

	void Logger::DisableCoreTags(std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		if (!s_CoreActiveTags.empty()) {
			for (const char* tag : tags) {
				if (IsCoreTagEnabled(tag)) {
					s_CoreActiveTags.erase(tag);
				}
			}
		}
	}

	bool Logger::IsCoreTagEnabled(std::string_view tag) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		return s_CoreActiveTags.contains(tag);
	}

	void Logger::ClearCoreTagFilters() {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		s_CoreActiveTags.clear();
	}


	void Logger::EnableClientTag(std::string_view tag) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		s_ClientActiveTags.insert(std::string(tag));
	}

	void Logger::EnableClientTags(std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		for (const char* tag : tags) {
			s_ClientActiveTags.insert(tag);
		}
	}

	void Logger::DisableClientTag(std::string_view tag) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		s_ClientActiveTags.erase(std::string(tag));
	}

	void Logger::DisableClientTags(std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		if (!s_ClientActiveTags.empty()) {
			for (const char* tag : tags) {
				if (s_ClientActiveTags.contains(tag)) {
					s_ClientActiveTags.erase(tag);
				}
			}
		}
	}

	bool Logger::IsClientTagEnabled(std::string_view tag) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		return s_ClientActiveTags.contains(tag);
	}

	void Logger::ClearClientTagFilters() {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		s_ClientActiveTags.clear();
	}


	bool Logger::ShouldCoreLog(std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_CoreTagMutex);
		if (s_CoreActiveTags.empty()) {
			return true;
		}
		for (const char* tag : tags) {
			if (s_CoreActiveTags.contains(tag)) {
				return true;
			}
		}
		return false;
	}

	bool Logger::ShouldClientLog(std::initializer_list<const char*> tags) {
		std::lock_guard<std::mutex> lock(s_ClientTagMutex);
		if (s_ClientActiveTags.empty()) {
			return true;
		}
		for (const char* tag : tags) {
			if (s_ClientActiveTags.contains(tag)) {
				return true;
			}
		}
		return false;
	}

}
