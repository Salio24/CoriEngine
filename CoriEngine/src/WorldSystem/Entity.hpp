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

		Entity(entt::handle handle) : m_EntityHandle(handle) {} // NOLINT

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
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Failed to get component(s) from entity. Entity does not have one or more of the following component type(s) '{}'", ([] {
					std::ostringstream oss;
					((oss << ", " << CORI_CLEAN_TYPE_NAME(T)), ...);
					return oss.str();
					})());
			}
			return m_EntityHandle.get<T...>();
		}

		template<typename... T>
		[[nodiscard]] decltype(auto) GetComponents() const {
			if (!HasComponents<T...>()) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Failed to get const component(s) from entity. Entity does not have one or more of the following component type(s) '{}'", ([] {
					std::ostringstream oss;
					((oss << ", " << CORI_CLEAN_TYPE_NAME(T)), ...);
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
		entt::handle::size_type RemoveComponents() { // NOLINT
			return m_EntityHandle.remove<T...>();
		}

		// erases the components without checking if an entity have they
		template<typename... T>
		void EraseComponents() { // NOLINT
			m_EntityHandle.erase<T...>();
		}

		explicit operator bool() const { return static_cast<bool>(m_EntityHandle); }

		bool operator==(const Entity& other) const {
			return m_EntityHandle == other.m_EntityHandle;
		}
		bool operator!=(const Entity& other) const {
			return m_EntityHandle != other.m_EntityHandle;
		}

		[[nodiscard]] bool IsValid() const {
			return static_cast<bool>(m_EntityHandle);
		}

		void SetActive(const bool state);

		[[nodiscard]] bool IsActiveLocally() const;

		[[nodiscard]] bool IsActiveGlobally() const;

		[[nodiscard]] uint32_t GetID() const {
			return entt::to_integral(m_EntityHandle.entity());
		}

		[[nodiscard]] uint32_t GetVersion() const {
			return entt::to_version(m_EntityHandle.entity());
		}

		[[nodiscard]] uint64_t GetEUID() const {
			return static_cast<uint64_t>(GetID()) << 32 | GetVersion();
		}

		[[nodiscard]] std::string GetDebugData(const bool showUUID = false) const;

		[[nodiscard]] std::expected<void, CoriError<>> SetParent(Entity parent);

		[[nodiscard]] std::expected<std::vector<Entity>, CoriError<>> GetSiblings() const;
		[[nodiscard]] std::expected<Entity, CoriError<>> GetParent() const;

		[[nodiscard]] std::expected<std::vector<Entity>, CoriError<>> GetChildren() const;

		[[nodiscard]] std::expected<Entity, CoriError<>> FindChildByName(const std::string& name) const;

		[[nodiscard]] entt::entity GetRawEntity() const { return m_EntityHandle.entity(); }

		void PrintHierarchy() const;

		std::string GetName() const;

		void SetName(const std::string& name);
	private:
		void UnlinkFromParent();
		void LinkToParent(Entity parent);

		static void DrawHierarchyRecursive(const Entity& entity, const std::string& prefix, const bool isLast);

		void UpdateInactivityFlagsRecursive(entt::entity parent, bool parentIsActive);

		entt::handle m_EntityHandle;

		friend class Scene;
	};
}