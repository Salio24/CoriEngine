#pragma once 
#include <entt/entt.hpp>

namespace Cori {
	class Scene;

	class Entity {
	public:

		// add const variants
		Entity() = default;

		Entity(entt::handle handle) : m_EntityHandle(handle) {}

		Entity(const entt::entity& entity);

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) {
			return m_EntityHandle.emplace<T>(std::forward<Args>(args)...);
		}

		template<typename T, typename... Args>
		T& ReplaceComponent(Args&&... args) {
			return m_EntityHandle.replace<T>(std::forward<Args>(args)...);
		}

		template<typename T, typename... Args>
		T& AddOrReplaceComponent(Args&&... args) {
			return m_EntityHandle.emplace_or_replace<T>(std::forward<Args>(args)...);
		}

		template<typename... T>
		decltype(auto) GetComponents() {
			if (!HasComponents<T...>()) {
				CORI_CORE_ERROR("Entity does not have component type {0}!", ([]() {
					std::ostringstream oss;
					((oss << ", " << typeid(T).name()), ...);
					return oss.str();
				})());
			}
			return m_EntityHandle.get<T...>();
		}

		template<typename... T>
		decltype(auto) GetComponents() const {
			if (!HasComponents<T...>()) {
				CORI_CORE_ERROR("Entity does not have component type {0} (const)!", ([]() {
					std::ostringstream oss;
					((oss << ", " << typeid(T).name()), ...);
					return oss.str();
				})());
			}

			return m_EntityHandle.get<const T...>();
		}

		template<typename... T>
		bool HasComponents() const {
			return m_EntityHandle.all_of<T...>();
		}

		template<typename... T>
		void RemoveComponents() {
			m_EntityHandle.remove<T...>();
		}

		template<typename... T>
		void EraseComponents() {
			m_EntityHandle.erase<T...>();
		}

		operator bool() const { return bool(m_EntityHandle); }

		bool operator==(const Entity& other) const {
			return m_EntityHandle == other.m_EntityHandle;
		}
		bool operator!=(const Entity& other) const {
			return m_EntityHandle != other.m_EntityHandle;
		}

		inline bool IsValid() const {
			return bool(this);
		} 

	private:
		entt::handle m_EntityHandle;

		friend class Scene;

		//for internal use ONLY
		static void SetViewScene(Scene* ptr);

	};
}