#pragma once
#include "Scene.hpp"
#include "TriggerManager.hpp"

namespace Cori {


	class SceneManager {
	public:

		static std::shared_ptr<Scene> CreateScene(const std::string& name) {
			return Get().CreateSceneImpl(name);
		}

		static void DestroyScene(const std::string& name) {
			Get().DestroySceneImpl(name);
		}

	protected:

		friend class Layer;

		std::shared_ptr<Scene> GetScene(const std::string& name) {
			CORI_CORE_ASSERT_FATAL(m_Scenes.contains(name), "No scene with name '{}' exists", name);
			return m_Scenes.at(name);
		}

	private:

		SceneManager() = default;
		~SceneManager() = default;

		std::shared_ptr<Scene> CreateSceneImpl(const std::string& name);

		void DestroySceneImpl(const std::string& name);

		static SceneManager& Get() {
			static SceneManager instance;
			return instance;
		}

		std::unordered_map<std::string, std::shared_ptr<Scene>> m_Scenes;
	};
}