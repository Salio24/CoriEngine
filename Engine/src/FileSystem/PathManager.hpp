#pragma once
#include <filesystem>
#include <unordered_map>

namespace Cori {
	namespace Core {
		class Application;
	}

	namespace FileSystem {
		/**
		 * @brief Path manager is responsible for loading the fsgame.json file from the drive, parsing it, and providing paths based on aliases.
		 * @details Example of fsgame.json:
		 * \code
		 * {
		 *   "paths": [
		 *     {
		 *       "alias": "APP_ROOT",
		 *       "path": "../../",
		 *       "root": "BIN"
		 *     },
		 *     {
		 *       "alias": "ENGINE_ROOT",
		 *       "path": "CoriEngine/Engine",
		 *       "root": "APP_ROOT"
		 *     },
		 *     {
		 *       "alias": "ENGINE_DATA",
		 *       "path": "enginedata",
		 *       "root": "ENGINE_ROOT"
		 *     },
		 *     {
		 *       "alias": "USER_DATA",
		 *       "path": "userdata",
		 *       "root": "APP_ROOT"
		 *     },
		 *     {
		 *       "alias": "ASSETS",
		 *       "path": "assets",
		 *       "root": "APP_ROOT"
		 *     },
		 *     {
		 *       "alias": "FONTS",
		 *       "path": "fonts",
		 *       "root": "ASSETS"
		 *     },
		 *     {
		 *       "alias": "LEVELS",
		 *       "path": "levels",
		 *       "root": "ASSETS"
		 *     },
		 *     {
		 *       "alias": "SOUNDS",
		 *       "path": "sounds",
		 *       "root": "ASSETS"
		 *     },
		 *     {
		 *       "alias": "TEXTURES",
		 *       "path": "textures",
		 *       "root": "ASSETS"
		 *     }
		 *   ]
		 * }
		 * \endcode
		 * By default you have alias 'BIN' defined, it points to the directory of the binary executable of your app.
		 * Cori expects you to define 'ENGINE_DATA' alias, it should point to the 'enginedata' path in the engine root folder.
		 * \n Let's examine how to define aliases correctly:
		 * \code
		 * {
		 *   "alias": "ASSETS",
		 *   // This the name of the alias, it is used to retrieve the alias from the code
		 *   // (with the GetAliasedPath method of this class) and to be a root for other alias defines
		 *   // later on in the json file.
		 *   "path": "assets",
		 *   // Folder inside the 'root' alias path to point to.
		 *   "root": "APP_ROOT"
		 *   // Alias that we will use as the root directory for out new alias.
		 * }
		 * \endcode
		 * When defining aliases you need to make sure the alias that you're trying to use as 'root' alias is defined above.
		 * \n Example:
		 * \n Incorrect:
		 * \code
		 * {
		 *   "alias": "ENGINE_ROOT",
		 *   "path": "CoriEngine/Engine",
		 *   "root": "APP_ROOT"
		 * },
		 * {
		 *   "alias": "APP_ROOT",
		 *   "path": "../../",
		 *   "root": "BIN"
		 * }
		 * \endcode
		 * Correct:
		 * \code
		 * {
		 *   "alias": "APP_ROOT",
		 *   "path": "../../",
		 *   "root": "BIN"
		 * },
		 * {
		 *   "alias": "ENGINE_ROOT",
		 *   "path": "CoriEngine/Engine",
		 *   "root": "APP_ROOT"
		 * }
		 * \endcode
		 * @note fsgame.json is expected to be 1 directory lower than the binary directory with the app executable.
		 */
		class PathManager {
		public:
			/**
			 * @brief Retries the full aliased path defined in fsgame.json
			 * @param alias Alias name to retrieve.
			 * @return If the alias exist it returns the full path taking into account alias root, if an alias is not found it will return an empty path.
			 */
			static std::filesystem::path GetAliasedPath(const std::string& alias);

			/**
			 * @brief Retries the full aliased path defined in fsgame.json
			 * @param alias Alias name to retrieve.
			 * @return If the alias exist it returns the full path taking into account alias root, if an alias is not found it will return an empty path.
			 */
			static std::filesystem::path GetAliasedPath(const std::string_view alias);

			/**
			 * @brief Retries the full aliased path defined in fsgame.json
			 * @param alias Alias name to retrieve.
			 * @return If the alias exist it returns the full path taking into account alias root, if an alias is not found it will return an empty path.
			 */
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
