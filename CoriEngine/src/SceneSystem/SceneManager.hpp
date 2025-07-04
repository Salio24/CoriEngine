#pragma once
#include "Scene.hpp"
#include "TriggerManager.hpp"

namespace Cori {
	class SceneManager {
	public:
		SceneManager() = default;
		~SceneManager() = default;

		static std::shared_ptr<Scene> CreateScene(const std::string& name);

		static std::shared_ptr<Scene> GetScene(const std::string& name);

		static void DestroyScene(const std::string& name);

	private:

		std::shared_ptr<Scene> GetSceneImpl(const std::string& name);
		std::shared_ptr<Scene> CreateSceneImpl(const std::string& name);

		void DestroySceneImpl(const std::string& name);

		std::unordered_map<std::string, std::shared_ptr<Scene>> m_Scenes;
	};
}