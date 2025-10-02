#pragma once
#include <entt/entt.hpp>
#include "Utility/StringHash.hpp"

namespace Cori {
	namespace World {
		/**
		 * @brief Entities are the essential part of WorldSystem.
		 * @details Entities can have a parent-children hierarchy that is represented by a left-child right-sibling binary tree internally.
		 * \n Instance of Entity class does not control a lifetime of an actual entity, it lifetime is controlled by the Scene. Through this class you interact with an actual entity, it is sort of a now-owning handle.
		 */
		class Entity {
		public:
			Entity() = default;

			Entity(entt::handle handle) : m_EntityHandle(handle) {} // NOLINT

			/**
			 * @brief Adds a component to the entity.
			 * @tparam T Type of component to add.
			 * @tparam Args Deduced automatically, no need to specify.
			 * @param args Arguments passed to the component constructor.
			 * @return A reference to the newly created component.
			 */
			template<typename T, typename... Args>
			T& AddComponent(Args&&... args) {
				return m_EntityHandle.emplace<T>(std::forward<Args>(args)...);
			}

			/**
			 * @brief Replaces the component with a newly created one.
			 * @tparam T Type of component to replace.
			 * @tparam Args Deduced automatically, no need to specify.
			 * @param args Arguments passed to the component constructor.
			 * @return A reference to the replaced component.
			 */
			template<typename T, typename... Args>
			T& ReplaceComponent(Args&&... args) {
				return m_EntityHandle.replace<T>(std::forward<Args>(args)...);
			}

			/**
			 * @brief Adds a component to the entity, or replaces it if the entity already has this component.
			 * @tparam T Type of component to add or replace.
			 * @tparam Args Deduced automatically, no need to specify.
			 * @param args Arguments passed to the component constructor.
			 * @return A reference to the newly created or replaced component.
			 */
			template<typename T, typename... Args>
			T& AddOrReplaceComponent(Args&&... args) {
				return m_EntityHandle.emplace_or_replace<T>(std::forward<Args>(args)...);
			}

			/**
			 * @brief Retries the references to the requested components of the entity.
			 * @tparam T Types of components to retrieve.
			 * @note When retrieving multiple components use structured binding. Also make sure you use receive by reference not by value.
			 * @return Referenced to the requested component(s).
			 */
			template<typename... T>
			[[nodiscard]] decltype(auto) GetComponents() {
				if (!HasComponents<T...>()) {
					CORI_CORE_FATAL_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Failed to get component(s) from entity. Entity '{}' does not have one or more of the following component type(s) '{}'", GetDebugData(), ([] {
						std::ostringstream oss;
						((oss << ", " << CORI_CLEAN_TYPE_NAME(T)), ...);
						return oss.str();
					})());
				}
				return m_EntityHandle.get<T...>();
			}

			/**
			 * @brief Retries the references to the requested components of the entity. Const variant.
			 * @tparam T Types of components to retrieve.
			 * @note When retrieving multiple components use structured binding. Also make sure you use receive by const reference not by value.
			 * @return Referenced to the requested component(s).
			 */
			template<typename... T>
			[[nodiscard]] decltype(auto) GetComponents() const {
				if (!HasComponents<T...>()) {
					CORI_CORE_FATAL_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Failed to get const component(s) from entity. Entity '{}' does not have one or more of the following component type(s) '{}'", GetDebugData(), ([] {
						std::ostringstream oss;
						((oss << ", " << CORI_CLEAN_TYPE_NAME(T)), ...);
						return oss.str();
					})());
				}

				return m_EntityHandle.get<const T...>();
			}

			/**
			 * @brief Retries or adds a component to the entity.
			 * @tparam T Type of component to retrieve or add if absent.
			 * @tparam Args Deduced automatically, no need to specify.
			 * @param args Arguments passed to the component constructor in case of addition of the component.
			 * @return Reference to the added or received component.
			 */
			template<typename T, typename... Args>
			T& GetOrAddComponent(Args&&... args) {
				return m_EntityHandle.get_or_emplace<T>(std::forward<Args>(args)...);
			}

			/**
			 * @brief Checks if the entity has all the specified components.
			 * @tparam T Components to check the presence of.
			 * @return True if the entity has all the components, false if at least one is absent.
			 */
			template<typename... T>
			[[nodiscard]] bool HasComponents() const {
				return m_EntityHandle.all_of<T...>();
			}

			/**
			 * @brief Removes components from the entity if entity has them.
			 * @tparam T Components to remove.
			 * @note Removes the components only if the entity has it, it's safe to try to remove a component that the entity doesn't have.
			 * @return Amount of components removed.
			 */
			template<typename... T>
			entt::handle::size_type RemoveComponents() { // NOLINT
				return m_EntityHandle.remove<T...>();
			}

			// erases the components without checking if an entity have they
			/**
			 * @brief Erases components from the entity without checking if the entity actually have them.
			 * @tparam T Components to erase.
			 * @warning This method is dangerous, call it only when you absolutely sure that the entity has all the components you're trying to erase, if at least one component is absent it will cause a crash.
			 */
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

			/**
			 * @brief Checks if the actual entity behind this handle is valid.
			 * @return True if valid, false otherwise.
			 */
			[[nodiscard]] bool IsValid() const {
				return static_cast<bool>(m_EntityHandle);
			}

			/**
			 * @brief Changes the activity state of the entity.
			 * @param state Activity state to set.
			 * @details When deactivating the entity a InactiveLocallyFlag and InactiveGloballyFlag are assigned to it, it this entity has any children a InactiveGloballyFlag will be added to the whole children tree recursively (to the grandchildren, grand-grandchildren and so on).
			 */
			void SetActive(const bool state);

			/**
			 * @brief Checks if the entity is active locally (doesn't have InactiveLocallyFlag), just a convenience function.
			 * @return True if it doesn't have InactiveLocallyFlag, false otherwise.
			 */
			[[nodiscard]] bool IsActiveLocally() const;

			/**
			 * @brief Checks if the entity is active locally (doesn't have InactiveGloballyFlag), just a convenience function.
			 * @return True if it doesn't have InactiveGloballyFlag, false otherwise.
			 */
			[[nodiscard]] bool IsActiveGlobally() const;

			/**
			 * @brief Gets the entity ID.
			 * @return Entity ID.
			 * @note Entity IDs can be reused, to differentiate one entity from the other, you need to compare entity ID and entity version. To get a unique entity ID use GetEUID() method.
			 */
			[[nodiscard]] uint32_t GetID() const {
				return entt::to_integral(m_EntityHandle.entity());
			}

			/**
			 * @brief Gets the entity version.
			 * @return Entity version.
			 */
			[[nodiscard]] uint32_t GetVersion() const {
				return entt::to_version(m_EntityHandle.entity());
			}

			/**
			 * @brief Gets the EUID (entity unique ID). A combination of entity ID and version, it is unique for every entity.
			 * @return EUID.
			 */
			[[nodiscard]] uint64_t GetEUID() const {
				return static_cast<uint64_t>(GetID()) << 32 | GetVersion();
			}

			[[nodiscard]] uint32_t GetOwnerSceneID() const {
				return m_OwningSceneID;
			}

			/**
			 * @brief Gets the debuting string for logging.
			 * @param showUUID Whether to include UUID in the debugging string.
			 * @return Formated debugging string.
			 */
			[[nodiscard]] std::string GetDebugData(bool showUUID = false) const;

			/**
			 * @brief Links the entity to a parent entity.
			 * @param parent Parent entity to link the entity to.
			 * @return Expected object with void on success or CoriError<> on failure.
			 * @warning It's illegal to call SetParent if entity or parent entity doesn't have Hierarchy and Name components, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			std::expected<void, Core::CoriError<>> SetParent(Entity parent);

			/**
			 * @brief Creates and returns a vector containing all entity siblings.
			 * @return Expected object with a vector of siblings, or CoriError<> on failure.
			 * @warning It's illegal to call GetSiblings if entity doesn't have Hierarchy and Name components, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			[[nodiscard]] std::expected<std::vector<Entity>, Core::CoriError<>> GetSiblings() const;

			/**
			 * @brief Retries the parent entity of the entity if any.
			 * @return Expected object with the parent entity, or CoriError<> on failure.
			 * @warning It's illegal to call GetParent if entity doesn't have Hierarchy and Name components, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			[[nodiscard]] std::expected<Entity, Core::CoriError<>> GetParent() const;

			/**
			 * @brief Creates and returns a vector containing all entity children (does not include grandchildren and so on, not recursive).
			 * @return Expected object with a vector of all children, or CoriError<> on failure.
			 * @warning It's illegal to call GetChildren if entity doesn't have Hierarchy and Name components, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			[[nodiscard]] std::expected<std::vector<Entity>, Core::CoriError<>> GetChildren() const;

			/**
			 * @brief Finds a children entity by name.
			 * @param name Name of the children to find.
			 * @return Expected object with child entity with the specified name, or CoriError<> on failure.
			 * @warning It's illegal to call FindChildByName if entity doesn't have Hierarchy and Name components, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			[[nodiscard]] std::expected<Entity, Core::CoriError<>> FindChildByName(const char* name) const;

			void DestroyChildren();

			/**
			 * @brief Gets a raw entt::entity if you need to interact with entt directly.
			 * @return Underlying entt::entity.
			 * @warning Use at your own risk and only if you know what you're doing.
			 */
			[[nodiscard]] entt::entity GetRawEntity() const { return m_EntityHandle.entity(); }

			/**
			 * @brief Gets a raw entt::handle if you need to interact with entt directly.
			 * @return Underlying entt::handle.
			 * @warning Use at your own risk and only if you know what you're doing.
			 */
			[[nodiscard]] entt::handle GetRawHandle() const { return m_EntityHandle; }

			/**
			 * @brief Prints the full entity hierarchy tree in the console.
			 * @warning It's illegal to call PrintHierarchy if entity doesn't have Hierarchy and Name components, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			void PrintHierarchy() const;

			/**
			 * @brief Retrieves the name of the entity.
			 * @return A view to the current entity name.
			 * @warning It's illegal to call GetName if entity doesn't have Name component, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			[[nodiscard]] std::string_view GetName() const;

			/**
			 * @brief Changes the entity name.
			 * @param name Name to change to.
			 * @warning It's illegal to call SetName if entity doesn't have Name component, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			void SetName(const std::string& name);

			/**
			 * @brief Unlinks the entity from its parent if it has one.
			 * @warning It's illegal to call UnlinkFromParent if entity doesn't have Hierarchy and Name components, this will lead to an assert in debug build and a crash in release.
			 * \n When creating an entity with Scene::CreateEntity you don't have to worry about it, applies only when you create an entity with Scene::CreateBlankEntity.
			 */
			void UnlinkFromParent();
		private:
			std::expected<void, Core::CoriError<>> LinkToParent(Entity parent);

			static void DrawHierarchyRecursive(const Entity& entity, const std::string& prefix, const bool isLast);

			void UpdateInactivityFlagsRecursive(entt::entity parent, bool parentIsActive);

			entt::handle m_EntityHandle;

			uint32_t m_OwningSceneID;

			friend class Scene;
		};
	}
}