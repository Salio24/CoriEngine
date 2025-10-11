#pragma once
#include "SceneHandle.hpp"

namespace Cori {
	namespace Core {
		class Application;
	}

	namespace World {
		/**
		 * @brief Responsible for creating and managing scenes, has full lifetime control of the existing scenes.
		 */
		class SceneManager {
		public:
			/**
			 * @brief Creates a scene with the specified name and adds it to the cache.
			 * @param name Name of the scene to create.
			 * @details Duplicate scene names are illegal.
			 * @return Expected object with a non owning handle to the created scene on success or a CoriError<> on failure.
			 */
			[[nodiscard]] static std::expected<SceneHandle, Core::CoriError<>> CreateScene(const std::string& name);

			/**
			 * @brief Retries the scene with the specified name from the cache.
			 * @param name Name of the scene to retrieve from cache.
			 * @return Expected object with a non owning handle to the created scene on success or a CoriError<> on failure.
			 */
			[[nodiscard]] static std::expected<SceneHandle, Core::CoriError<>> GetScene(const std::string& name);

			/**
			 * @brief Retries the scene with the specified name from the cache.
			 * @param name Name of the scene to retrieve from cache.
			 * @return Expected object with a non owning handle to the created scene on success or a CoriError<> on failure.
			 */
			[[nodiscard]] static std::expected<SceneHandle, Core::CoriError<>> GetScene(const std::string_view name);

			/**
			 * @brief Allows you to get SceneHandle if all you know is scene id.
			 * @details Each scene is assigned an ID at creation, entities have this ID and you can ask an entity what scene is it from, it will give you the owner scene id,
			 * you can "convert" this scene id into a useful handle here.
			 * @param sceneID ID of the scene to get a handle for.
			 * @return Expected object with SceneHandle on success, CoriError on failure.
			 */
			[[nodiscard]] static std::expected<SceneHandle, Core::CoriError<>> GetHandle(const uint32_t sceneID);

			/**
			 * @brief Destroys a scene with the specified name.
			 * @param name Name of the scene to delete.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			[[nodiscard]] static std::expected<void, Core::CoriError<>> DestroyScene(const std::string& name);

		private:
			friend Core::Application;
			static void Init();
			static void Shutdown();

			struct Data;
			static Data* s_Data;
		};
	}
}