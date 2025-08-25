#pragma once
#include "Scene.hpp"

namespace Cori {
	class SceneManager {
	public:
		static void Init();

		static void Shutdown();

		[[nodiscard]] static std::expected<std::shared_ptr<Scene>, CoriError<>> CreateScene(const std::string& name);

		[[nodiscard]] static std::expected<std::shared_ptr<Scene>, CoriError<>> GetScene(const std::string& name);

		[[nodiscard]] static std::expected<void, CoriError<>> DestroyScene(const std::string& name);

	private:
		struct Data;
		static Data* s_Data;
	};
}