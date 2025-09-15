#pragma once
#include "Scene.hpp"

namespace Cori {
	namespace Core {
		class Layer;
	}

	namespace World {
		class SceneHandle {
		public:
			SceneHandle() = default;
			~SceneHandle() = default;

			void OnUpdate(const double deltaTime) {
				if (m_SceneRaw) {
					m_SceneRaw->OnUpdate(deltaTime);
				}
			}

			void OnTickUpdate(const float timeStep) {
				if (m_SceneRaw) {
					m_SceneRaw->OnTickUpdate(timeStep);
				}
			}

			[[nodiscard]] Entity CreateEntity(const std::string& name, const Utility::HashedTag64& tag) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->CreateEntity(name, tag);
			}

			void DestroyEntity(Entity entity) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				m_SceneRaw->DestroyEntity(entity);
			}

			std::expected<void, Core::CoriError<>> AddEntityToCache(Entity entity, const Utility::StringHash32 tag) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->AddEntityToCache(entity, tag);
			}

			[[nodiscard]] std::expected<Entity, Core::CoriError<>> GetEntityFromCache(const Utility::StringHash32 tag) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->GetEntityFromCache(tag);
			}

			void RemoveEntityFromCache(const Utility::StringHash32 key) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				m_SceneRaw->RemoveEntityFromCache(key);
			}

			[[nodiscard]] std::expected<Entity, Core::CoriError<>> FindEntity(const std::string& name) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->FindEntity(name);
			}

			[[nodiscard]] std::expected<Entity, Core::CoriError<>> FindEntity(const std::string& name, const Utility::HashedTag64& tag) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->FindEntity(name, tag);
			}

			template<typename... T>
			[[nodiscard]] auto View() {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->View<T...>();
			}

			template<typename... T, typename... ExcludeT>
			[[nodiscard]] auto View(Exclude<ExcludeT...> exclude_list) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->View<T...>(exclude_list);
			}

			//template<typename... T, typename Func>
			//void ForEach(Func func) {
			//	CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
			//	m_SceneRaw->ForEach<T...>(func);
			//}

			template<typename T, typename... Args>
			T& AddContextComponent(Args&&... args) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->AddContextComponent<T>(std::forward<Args>(args)...);
			}

			template<typename T, typename... Args>
			T& AddOrAssignContextComponent(Args&&... args) {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->AddOrAssignContextComponent<T>(std::forward<Args>(args)...);
			}

			template<typename T>
			[[nodiscard]] T& GetContextComponent() {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->GetContextComponent<T>();
			}

			template<typename T>
			[[nodiscard]] const T& GetContextComponent() const {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->GetContextComponent<T>();
			}

			template<typename T>
			[[nodiscard]] bool HasContextComponent() const {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->HasContextComponent<T>();
			}

			template<typename T>
			void RemoveContextComponent() {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				m_SceneRaw->RemoveContextComponent<T>();
			}

			[[nodiscard]] Graphics::CameraController& GetActiveCamera() {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->m_ActiveCamera;
			}

			[[nodiscard]] const Graphics::CameraController& GetActiveCamera() const {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->m_ActiveCamera;
			}

			[[nodiscard]] Physics::PhysicsWorld& GetPhysicsWorld() {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->m_PhysicsWorld;
			}

			[[nodiscard]] const Physics::PhysicsWorld& GetPhysicsWorld() const {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->m_PhysicsWorld;
			}

			[[nodiscard]] std::string_view GetName() const {
				CORI_CORE_ASSERT(m_SceneRaw != nullptr, "No scene is currently bound.");
				return m_SceneRaw->m_Name;
			}

			[[nodiscard]] bool IsValid() const {
				return m_SceneRaw != nullptr;
			}

		protected:
			friend Core::Layer;
			[[nodiscard]] bool OnUnbind() {
				if (m_SceneRaw != nullptr) {
					return m_SceneRaw->OnUnbind();
				}
				return false;
			}

			std::shared_ptr<Scene> m_SceneRaw;
		};
	}
}