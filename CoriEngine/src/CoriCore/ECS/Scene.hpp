#pragma once
#include "Entity.hpp"
#include "../SelfFactory.hpp"
#include "../Profiling/Trackable.hpp"
#include "../Renderer/CameraController.hpp"
#include "../EventSystem/Event.hpp"

namespace Cori {

	template<typename... T>
	inline constexpr auto& Exclude = entt::exclude<T...>;

	class Scene : public Profiling::Trackable<Scene>, public SharedSeflFactory<Scene> {
	public:
		static bool PreCreateHook([[maybe_unused]] const std::string& name) { return true; }
		~Scene();

		bool OnBind(const EventCallbackFn& callback);
		bool OnUnbind();

		void OnUpdate(const double deltaTime);

		Entity CreateEntity(const std::string& name);
		Entity CreateEntity();

		Entity GetNamedEntity(const std::string& name);

		Entity EntityFromEnTT(const entt::entity& entity) {
			return Entity{ entt::handle{m_Registry, entity} };
		}

		// and const variants
		template<typename... T, typename... Args>
		auto View(Args&& ... args) {
			return m_Registry.view<T...>(std::forward<Args>(args)...);
		}

		template<typename... T, typename Func>
		void ForEach(Func func) {
			m_Registry.view<T...>().each(func);
		}

		template<typename T, typename... Args>
		T& AddContextComponent(Args&&... args) {
			return m_Registry.ctx().emplace<T>(std::forward<Args>(args)...);
		}

		template<typename T, typename... Args>
		T& AddOrAssignContextComponent(Args&&... args) {
			return m_Registry.ctx().insert_or_assign<T>(std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetContextComponent() {
			return m_Registry.ctx().get<T>();
		}

		template<typename T>
		const T& GetContextComponent() const {
			return m_Registry.ctx().get<const T>();
		}

		template<typename T>
		bool HasContextComponent() const {
			return m_Registry.ctx().contains<T>();
		}

		template<typename T>
		void RemoveContextComponent() {
			m_Registry.ctx().erase<T>();
		}

		CameraController ActiveCamera;

		EventCallbackFn m_TriggerEventCallback;

		std::string m_Name;
	protected:
		Scene(const std::string& name);
	private:
		entt::registry m_Registry;


		std::unordered_map<std::string, entt::handle> m_NamedEntities;

	};
}
