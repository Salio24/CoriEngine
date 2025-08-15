#pragma once
#include "Scene.hpp"

namespace Cori {
	class SceneManager {
	public:
		static void Init();

		static void Shutdown();

		static std::shared_ptr<Scene> CreateScene(const std::string& name);

		static std::shared_ptr<Scene> GetScene(const std::string& name);

		static void DestroyScene(const std::string& name);

	private:
		struct Data;
		static Data* s_Data;
	};
}