#pragma once
#include <filesystem>
#include <unordered_map>

namespace Cori {
	namespace Core {
		class Application;
	}
	namespace FileSystem {
		class PathManager {
		public:
			static std::filesystem::path GetAliasedPath(const std::string& alias);
			static std::filesystem::path GetAliasedPath(const std::string_view alias);
			static std::filesystem::path GetAliasedPath(const char* alias);

		private:
			PathManager();

			static PathManager& Get() {
				static PathManager s_Instance;
				return s_Instance;
			}

			friend Core::Application;

			struct TransparentHash {
				using is_transparent = void;
				size_t operator()(std::string_view sv) const noexcept {
					return std::hash<std::string_view>{}(sv);
				}
			};

			struct TransparentEqual {
				using is_transparent = void;
				bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
					return lhs == rhs;
				}
			};

			std::unordered_map<std::string, std::filesystem::path, TransparentHash, TransparentEqual> m_AliasedPaths;
		};
	}
}
