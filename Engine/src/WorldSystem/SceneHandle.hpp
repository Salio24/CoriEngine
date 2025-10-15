#pragma once
#include "Scene.hpp"

namespace Cori {
	namespace Core {
		class Layer;
	}

	namespace World {
		/**
		 * @brief A handle for the scene, checks for scene validity before any call to the scene, if scene is invalid asserts. Will not keep the scene alive.
		 */
		class SceneHandle {
		public:
			/**
			 * @brief Creates a handle for the scene.
			 * @param scene Scene to create a handle for.
			 */
			SceneHandle(const std::shared_ptr<Scene>& scene) : m_SceneRaw(scene) {} //NOLINT

			/**
			 * @brief Creates a blank Entity with no components attached.
			 * @return A handle to the blank entity.
			 * @warning Be very carefully when using entities that don't have a default set of components, when you create an entity with CreateEntity it has <Name, Hierarchy, UUID, Transform> components by default.
			 * \n Some engine systems expect an entity to have some of those components.
			 * \n Entity::SetName() and Entity::GetName() expects Entity to have a Name component.
			 * \n Anything connected to the Entity hierarchy system expects an Entity to have Hierarchy and Name components.
			 * \n Be aware and use carefully, can cause a crash if used incorrectly.
			 */
			[[nodiscard]] Entity CreateBlankEntity() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->CreateBlankEntity();
			}

			/**
			 * @brief Creates an Entity with a default set of components.
			 * @param name Name to assign to the Entity.
			 * @tparam T Optional set of components or tags that will be added to the entity at creation.
			 * @return A handle to the created entity.
			 * @details Unlike Scene::CreateBlankEntity() it is safe to use an Entity created with this method anywhere you like. Default set of components is <Name, Hierarchy, UUID, Transform>.
			 */
			template<typename... T>
			[[nodiscard]] Entity CreateEntity(const std::string& name) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->CreateEntity<T...>(name);
			}

			/**
			 * @brief Destroys the entity.
			 * @param entity Handle of the entity to destroy.
			 */
			void DestroyEntity(Entity entity) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				m_SceneRaw.lock()->DestroyEntity(entity);
			}

			/**
			 * @brief Adds entity to the local scene cache.
			 * @param entity Handle of the entity to add to cache.
			 * @param key 32bit FNV-1a hashed string, you can use ""_hs32 operator to create a compile time hash.
			 * \n The only purpose of a scene local entity cache is to store entities that you plan to access very frequently but don't have a good place on the client side to store the handle.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> AddEntityToCache(Entity entity, const Utility::StringHash32 key) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->AddEntityToCache(entity, key);
			}

			/**
			 * @brief Retries the entity from a scene local entity cache.
			 * @param key Key you associated with an entity when adding it via AddEntityToCache method.
			 * @return Expected object with an entity handle on success, or CoriError<> on failure.
			 */
			[[nodiscard]] std::expected<Entity, Core::CoriError<>> GetEntityFromCache(const Utility::StringHash32 key) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->GetEntityFromCache(key);
			}

			/**
			 * @brief Removes an entity from the scene local entity cache.
			 * @param key Key you associated with an entity when adding it via AddEntityToCache method.
			 */
			void RemoveEntityFromCache(const Utility::StringHash32 key) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				m_SceneRaw.lock()->RemoveEntityFromCache(key);
			}

			/**
			 * @brief Conducts a scene wide search for the entity with a particular name and optionally a set of components.
			 * @param name Name of the entity you're searching for.
			 * @tparam T Optional set of components or tags the desired entity should be filtered by.
			 * @note As the entity names doesn't have to be unique, it returns the first entity with the given name and set of components it finds.
			 * @return Expected object with an entity handle on success, or CoriError<> on failure.
			 * @warning This is pretty slow as it is a scene wide linear search and absolutely should not be used in a tight loop, each frame/tick or during gameplay!
			 */
			template<typename... T>
			[[nodiscard]] std::expected<Entity, Core::CoriError<>> FindEntity(const std::string& name) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->FindEntity<T...>(name);
			}

			/**
			 * @brief Constructs a view of the entities that have a particular set of components. Variant without component exclusion.
			 * @tparam T A set of components of the entities in the view.
			 * @details Example usage: auto view = View<ComponentOne, ComponentTwo>();
			 * \n This way the view will consist of entities that both have <ComponentOne, ComponentTwo> components.
			 * @return Newly constructed view.
			 */
			template<typename... T>
			[[nodiscard]] auto StaticView() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->StaticView<T...>();
			}

			/**
			 * @brief Constructs a static view of the entities that have a particular set of components. Variant with component exclusion.
			 * @tparam T A set of components of the entities in the view.
			 * @tparam ExcludeT Component types to exclude from the view.
			 * @param excludeList Instance of Exclude with ExcludeT components.
			 * @details Example usage: auto view = View<ComponentOne, ComponentTwo>(Exclude<FlagOne, FlagTwo>());
			 * \n This way the view will consist of entities that both have <ComponentOne, ComponentTwo> components, and at the same time don't have either FlagOne or FlagTwo.
			 * @return Newly constructed view.
			 */
			template<typename... T, typename... ExcludeT>
			[[nodiscard]] auto StaticView(Exclude<ExcludeT...> excludeList) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->StaticView<T...>(excludeList);
			}

			/**
			 * @brief Constructs a dynamic view which can be configured at runtime.
			 * @details Use this when the exact set of components a view should have is not known at compile time.
			 * @note Dynamic views are slightly slower to iterate than static views.
			 * @return An instance of dynamic entity view for the creator scene.
			 */
			[[nodiscard]] DynamicEntityView DynamicView() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->DynamicView();
			}

			//template<typename... T, typename Func>
			//void ForEach(Func func) {
			//	CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
			//	m_SceneRaw.lock()->ForEach<T...>(func);
			//}

			/**
			 * @brief Adds a component to the scene.
			 * @tparam T Type of component to add.
			 * @tparam Args Deduced automatically, no need to specify.
			 * @param args Arguments passed to the component constructor.
			 * @return A reference to the newly created component.
			 */
			template<typename T, typename... Args>
			T& AddContextComponent(Args&&... args) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->AddContextComponent<T>(std::forward<Args>(args)...);
			}

			/**
			 * @brief Retries the reference to the requested context component.
			 * @tparam T Type of context component to retrieve.
			 * @note Make sure you use receive by reference not by value.
			 * @return Referenced to the requested context component.
			 */
			template<typename T>
			[[nodiscard]] T& GetContextComponent() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->GetContextComponent<T>();
			}

			/**
			 * @brief Retries the reference to the requested context component. Const variant.
			 * @tparam T Type of context component to retrieve.
			 * @note Make sure you use receive by const reference not by value.
			 * @return Referenced to the requested context component.
			 */
			template<typename T>
			[[nodiscard]] const T& GetContextComponent() const {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->GetContextComponent<T>();
			}

			/**
			 * @brief Checks if a scene has a specific context component.
			 * @tparam T Component type to check.
			 * @return True the scene has this context component, false otherwise.
			 */
			template<typename T>
			[[nodiscard]] bool HasContextComponent() const {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->HasContextComponent<T>();
			}

			/**
			 * @brief Removes a context component from the scene.
			 * @tparam T Context component type to remove.
			 * @note Removes the context components only if the scene has it, it's safe to try to remove a context component that the scene doesn't have.
			 */
			template<typename T>
			void RemoveContextComponent() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				m_SceneRaw.lock()->RemoveContextComponent<T>();
			}

			/**
			 * @brief Registers the system for the scene.
			 * @tparam T System to register.
			 * @tparam Args Deduced automatically, no need to specify.
			 * @param args Arguments that will be passed to Create method of your system class.
			 * @details Scene has full control of the system lifetime, the system will be kept alive for as long as the scene is alive, but you can also explicitly unregister the system.
			 * @note If Create returns false, the system will not be registered.
			 */
			template <typename T, typename... Args> requires IsSystem<T>
			void RegisterSystem(Args&&... args) {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				m_SceneRaw.lock()->RegisterSystem<T>(std::forward<Args>(args)...);
			}

			/**
			 * @brief Unregisters the system from the scene.
			 * @tparam T System to unregister.
			 * @note It is safe to call this method with a T system that is not registered.
			 */
			template <typename T> requires IsSystem<T>
			void UnregisterSystem() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				m_SceneRaw.lock()->UnregisterSystem<T>();
			}

			/**
			 * @brief Retries a registered system instance from the scene.
			 * @tparam T System to retrieve.
			 * @return Weak pointer to the requested system instance.
			 */
			template <typename T> requires IsSystem<T>
			[[nodiscard]] std::expected<std::weak_ptr<T>, Core::CoriError<>> GetSystem() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->GetSystem<T>();
			}

			/**
			 * @brief Retrieves a reference to the CameraController associated with the current camera.
			 * @return Reference to the CameraController.
			 */
			[[nodiscard]] Graphics::CameraController& GetActiveCamera() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->GetCameraController();
			}

			/**
			 * @brief Retrieves a const reference to the CameraController associated with the current camera.
			 * @return Const reference to the CameraController.
			 */
			[[nodiscard]] const Graphics::CameraController& GetActiveCamera() const {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->GetCameraController();
			}

			/**
			 * @brief Retrieves the name of the scene.
			 * @return View to the name of the scene.
			 */
			[[nodiscard]] std::string_view GetName() const {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->GetName();
			}

			/**
			 * @brief Retrieves the EnTT registry.
			 * @return Non-const reference to the EnTT registry.
			 * @details For the most part this is needed if your system need some special behaviour/feature (listeners, reactive storage, etc.) from EnTT that I don't have abstraction over.
			 * \n Making a feature complete wrapper over EnTT is out of the scope of this project.
			 */
			[[nodiscard]] entt::registry& GetRegistry() {
				CORI_CORE_ASSERT(!m_SceneRaw.expired(), "No scene is currently bound.");
				return m_SceneRaw.lock()->m_Registry;
			}

			/**
			 * @brief Checks if the scene the handle points to is still valid.
			 * @return True if scene is valid, false otherwise.
			 */
			[[nodiscard]] bool IsValid() const {
				return !m_SceneRaw.expired();
			}

		protected:
			friend Core::Layer;
			friend Core::Application;

			void BeginRender() {
				if (!m_SceneRaw.expired()) {
					m_SceneRaw.lock()->BeginRender();
				}
			}

			void EndRender() {
				if (!m_SceneRaw.expired()) {
					m_SceneRaw.lock()->EndRender();
				}
			}

			void OnUpdate(Core::GameTimer& gameTimer) {
				if (!m_SceneRaw.expired()) {
					m_SceneRaw.lock()->OnUpdate(gameTimer);
				}
			}

			void OnTickUpdate(Core::GameTimer& gameTimer) {
				if (!m_SceneRaw.expired()) {
					m_SceneRaw.lock()->OnTickUpdate(gameTimer);
				}
			}

			void OnImGuiRender(Core::GameTimer& gameTimer) {
				if (!m_SceneRaw.expired()) {
					m_SceneRaw.lock()->OnImGuiRender(gameTimer);
				}
			}

			[[nodiscard]] bool OnUnbind() {
				if (!m_SceneRaw.expired()) {
					return m_SceneRaw.lock()->OnUnbind();
				}
				return false;
			}

			[[nodiscard]] bool OnBind() {
				if (!m_SceneRaw.expired()) {
					return m_SceneRaw.lock()->OnBind();
				}
				return false;
			}

		private:
			std::weak_ptr<Scene> m_SceneRaw;
		};
	}
}