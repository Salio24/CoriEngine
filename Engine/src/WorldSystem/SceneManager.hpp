#pragma once
#include "Scene.hpp"

namespace Cori {
	namespace Core {
		class Application;
	}

	namespace World {
		class SceneManager {
		public:
			[[nodiscard]] static std::expected<std::shared_ptr<Scene>, Core::CoriError<>> CreateScene(const std::string& name);

			[[nodiscard]] static std::expected<std::shared_ptr<Scene>, Core::CoriError<>> GetScene(const std::string& name);

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