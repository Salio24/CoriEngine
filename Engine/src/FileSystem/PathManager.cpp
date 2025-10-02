#include "PathManager.hpp"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace Cori {
	namespace FileSystem {
		std::filesystem::path PathManager::GetAliasedPath(const std::string& alias) {
			if (Get().m_AliasedPaths.contains(alias)) {
				return Get().m_AliasedPaths[alias];
			}

			if (Logger::GetStatus()) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Failed to retrieve path for alias '{}', returned empty path.", alias);
			} else {
				std::println(stderr, "[ERROR] Failed to retrieve path for alias '{}', returned empty path.", alias);
			}

			return {};
		}

		std::filesystem::path PathManager::GetAliasedPath(const std::string_view alias) {
			if (Get().m_AliasedPaths.contains(alias)) {
				return Get().m_AliasedPaths.find(alias)->second;
			}

			if (Logger::GetStatus()) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Failed to retrieve path for alias '{}', returned empty path.", alias);
			} else {
				std::println(stderr, "[ERROR] Failed to retrieve path for alias '{}', returned empty path.", alias);
			}

			return {};
		}

		std::filesystem::path PathManager::GetAliasedPath(const char* alias) {
			if (Get().m_AliasedPaths.contains(alias)) {
				return Get().m_AliasedPaths.find(alias)->second;
			}

			if (Logger::GetStatus()) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Failed to retrieve path for alias '{}', returned empty path.", alias);
			} else {
				std::println(stderr, "[ERROR] Failed to retrieve path for alias '{}', returned empty path.", alias);
			}

			return {};
		}

		PathManager::PathManager() {
			m_AliasedPaths.insert({"BIN", ""});

			std::filesystem::path pathFile = "../fsgame.json";

			try {
				std::ifstream f(pathFile);
				if (Logger::GetStatus()) {
					CORI_CORE_INFO_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Loading global path definitions from: {}", pathFile.string());
				} else {
					std::println("Loading global path definitions from: {}", pathFile.string());
				}
				if (!f.good()) {
					throw Core::CoriError(std::format("Failed to open json file {}", pathFile.string()));
				}

				json data = json::parse(f);

				const json& paths = data["paths"];

				for (const auto& path : paths) {
					if (path.contains("alias") && path.contains("path") && path.contains("root")) {
						std::string root = path["root"];

						if (Logger::GetStatus()) {
							CORI_CORE_ASSERT(m_AliasedPaths.contains(root), "Path for alias '{}' was not defined before it was used.", root);
						} else {
							if (!m_AliasedPaths.contains(root)) {
								const std::source_location& loc = std::source_location::current();
								std::println(stderr,
											 "[FATAL] {}:{}:{} in {}(): Path for alias '{}' was not defined before it was used.",
											 loc.file_name(),
											 loc.line(),
											 loc.column(),
											 loc.function_name(),
											 root);
								std::abort();
							}
						}

						std::string alias = path["alias"];
						std::filesystem::path rootPath = m_AliasedPaths[root];
						std::filesystem::path aliasPath = path["path"];
						std::filesystem::path fullPath = rootPath / aliasPath;
						m_AliasedPaths.insert({alias, fullPath});
					} else {
						if (Logger::GetStatus()) {
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Failed to load an entry from \"paths\" array, entry doesn't contain on of following fields: \"alias\", \"path\", \"root\"");
						} else {
							std::println(stderr, "[ERROR] Failed to load an entry from \"paths\" array, entry doesn't contain on of following fields: \"alias\", \"path\", \"root\"");
						}
					}
				}
			}
			catch (std::exception& e) {
				if (Logger::GetStatus()) {
					CORI_CORE_FATAL_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Encountered an error when trying to parse global path config '{}', this can blow any time now. \nError: {}", pathFile.string(), e.what());
				} else {
					std::println(stderr, "[FATAL] Encountered an error when trying to parse global path config '{}', this can blow any time now. \nError: {}", pathFile.string(), e.what());
				}
			}
		}

		#if 0
		void PathManager::Init(const std::filesystem::path& pathFile) {
			if (!s_Data) {
				s_Data = new Data();

				s_Data->m_AliasedPaths.insert({"BIN", ""});

				try {
					std::ifstream f(pathFile);
					CORI_CORE_INFO_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Loading global path definitions from: {}", pathFile.string());
					if (!f.good()) {
						throw Core::CoriError(std::format("Failed to open json file {}", pathFile.string()));
					}

					json data = json::parse(f);

					const json& paths = data["paths"];

					for (const auto& path : paths) {
						if (path.contains("alias") && path.contains("path") && path.contains("root")) {
							std::string root = path["root"];

							CORI_CORE_ASSERT(s_Data->m_AliasedPaths.contains(root), "Path for alias '{}' was not defined before it was used.", root);

							std::string alias = path["alias"];
							std::filesystem::path rootPath = s_Data->m_AliasedPaths[root];
							std::filesystem::path aliasPath = path["path"];
							std::filesystem::path fullPath = rootPath / aliasPath;
							s_Data->m_AliasedPaths.insert({alias, fullPath});
						} else {
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Failed to load an entry from \"paths\" array, entry doesn't contain on of following fields: \"alias\", \"path\", \"root\"");
						}
					}
				}
				catch (std::exception& e) {
					CORI_CORE_FATAL_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::PathManager }, "Encountered an error when trying to parse global path config '{}', this can blow any time now. Error: {}", pathFile.string(), e.what());
				}
			}
		}
		#endif

		//void PathManager::Shutdown() {
		//	if (s_Data) {
		//		delete s_Data;
		//	}
		//}
	}
}