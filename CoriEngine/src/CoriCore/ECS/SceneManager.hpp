#pragma once
#include "Scene.hpp"
#include "TriggerManager.hpp"

namespace Cori {


	class SceneManager {
	public:

		static Scene* CreateScene(const std::string& name) {
			return Get().CreateSceneImpl(name);
		}

		static void DestroyScene(const std::string& name) {
			Get().DestroySceneImpl(name);
		}

		static void BindScene(const std::string& name) {
			Get().BindSceneImpl(name);
		}

		static void UnbindScene() {
			Get().UnbindSceneImpl();
		}

		struct ActiveScene {

			inline static Entity CreateEntity(const std::string& name) {
				return Get().GetActiveScene()->CreateEntity(name);
			}

			inline static Entity CreateEntity() {
				return Get().GetActiveScene()->CreateEntity();
			}

			inline static Entity GetNamedEntity(const std::string& name) {
				return Get().GetActiveScene()->GetNamedEntity(name);
			}

			template<typename... T, typename... Args>
			inline static auto View(Args&& ... args) {
				return Get().GetActiveScene()->View<T...>(std::forward<Args>(args)...);
			}

			template<typename... T, typename Func>
			inline static void ForEach(Func func) {
				Get().GetActiveScene()->ForEach<T...>(std::forward<Func>(func));
			}

			template<typename T, typename... Args>
			inline static T& AddContextComponent(Args&&... args) {
				return Get().GetActiveScene()->AddContextComponent<T>(std::forward<Args>(args)...);
			}

			template<typename T, typename... Args>
			inline static T& AddOrAssignContextComponent(Args&&... args) {
				return Get().GetActiveScene()->AddOrAssignContextComponent<T>(std::forward<Args>(args)...);
			}

			template<typename T>
			inline static T& GetContextComponent() {
				return Get().GetActiveScene()->GetContextComponent<T>();
			}

			template<typename T>
			inline static const T& GetContextComponent() {
				return Get().GetActiveScene()->GetContextComponent<const T>();
			}

			template<typename T>
			inline static bool HasContextComponent() {
				return Get().GetActiveScene()->HasContextComponent<const T>();
			}

			template<typename T>
			inline static void RemoveContextComponent() {
				Get().GetActiveScene()->RemoveContextComponent<const T>();
			}
		};
	protected:
		friend class Application;

		static void OnUpdate(const double deltaTime) {
#ifdef DEBUG_BUILD
			if (Get().m_ActiveScene != nullptr ) {
#endif
				TriggerManager::Get().OnUpdate(deltaTime, Get().m_ActiveScene);
				Get().m_ActiveScene->OnUpdate(deltaTime);

#ifdef DEBUG_BUILD
			}
#endif
		}

	private:

		inline Scene* GetActiveScene() {
			CORI_CORE_ASSERT_FATAL(m_ActiveScene != nullptr, "SceneManager::ActiveScene: Active scene requested but no scene is bound!");
			return m_ActiveScene;
		}

		SceneManager() = default;
		~SceneManager() = default;

		Scene* CreateSceneImpl(const std::string& name);

		void DestroySceneImpl(const std::string& name);

		void BindSceneImpl(const std::string& name);

		void UnbindSceneImpl();

		static SceneManager& Get() {
			static SceneManager instance;
			return instance;
		}


		std::unordered_map<std::string, std::unique_ptr<Scene>> m_Scenes;
		Scene* m_ActiveScene{ nullptr };
	};
}