#pragma once 
#include <entt/entt.hpp>
#include "Core/Utility/StringHash.hpp"


namespace Cori {
	class Scene;

	class Entity {
	public:

		// add const variants
		// add check to validation of entity validity in methods
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
		[[nodiscard]] decltype(auto) GetComponents() {
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
		[[nodiscard]] decltype(auto) GetComponents() const {
			if (!HasComponents<T...>()) {
				CORI_CORE_ERROR("Entity does not have component type {0} (const)!", ([]() {
					std::ostringstream oss;
					((oss << ", " << typeid(T).name()), ...);
					return oss.str();
				})());
			}

			return m_EntityHandle.get<const T...>();
		}

		template<typename... T, typename... Args>
		decltype(auto) GetOrAddComponent(Args&&... args) {
			return m_EntityHandle.get_or_emplace<T...>(std::forward<Args>(args)...);
		}

		template<typename... T>
		bool HasComponents() const {
			return m_EntityHandle.all_of<T...>();
		}

		// erases the components only when an entity have they, otherwise does nothing
		template<typename... T>
		void RemoveComponents() {
			m_EntityHandle.remove<T...>();
		}

		// erases the components without checking if an entity have they
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

		[[nodiscard]] bool IsValid() const {
			return bool(m_EntityHandle);
		}

		void SetActive(bool state);

		[[nodiscard]] bool IsActiveLocally() const;

		[[nodiscard]] bool IsActiveGlobally() const;

		[[nodiscard]] uint32_t GetID() const {
			return entt::to_integral(m_EntityHandle.entity());
		}

		[[nodiscard]] uint32_t GetVersion() const {
			return entt::to_version(m_EntityHandle.entity());
		}

		[[nodiscard]] uint64_t GetEUID() const {
			return (static_cast<uint64_t>(GetID()) << 32) | GetVersion();
		}

		//[[nodiscard]] std::string GetDebuggingUID() const {
		//	return "(Entity ID: " + std::to_string(GetID()) + ", Version: " + std::to_string(GetVersion()) + ")";
		//}

		[[nodiscard]] std::string GetDebugData(bool showUUID = false) const;

		[[nodiscard]] std::expected<void, const char*>  SetParent(Entity parent);

		[[nodiscard]] std::expected<std::vector<Entity>, const char*> GetSiblings() const;
		[[nodiscard]] std::expected<Entity, const char*> GetParent() const;

		[[nodiscard]] std::expected<std::vector<Entity>, const char*> GetChildren() const;

		[[nodiscard]] std::expected<Entity, const char*> FindChildByName(const std::string& name) const;

		// rename this to GetRawEntity
		[[nodiscard]] entt::entity GetRawEntity() const { return m_EntityHandle.entity(); }

		void PrintHierarchy();

		std::string GetName();

		void SetName(const std::string& name);
	private:

		void UnlinkFromParent();
		void LinkToParent(Entity parent);

		void DrawHierarchyRecursive(Entity entity, const std::string& prefix, bool isLast);

		void UpdateInactivityFlagsRecursive(entt::entity parent, bool parentIsActive);

		entt::handle m_EntityHandle;

		friend class Scene;
	};
}