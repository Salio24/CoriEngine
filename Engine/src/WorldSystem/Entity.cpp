#include "Entity.hpp"
#include "Scene.hpp"
#include "Systems/Hierarchy.hpp"

namespace Cori {
	namespace World {
		void Entity::SetActive(const bool state) {
			if (state) {
				if (HasComponents<Components::Entity::InactiveLocallyFlag>()) {
					EraseComponents<Components::Entity::InactiveLocallyFlag>();
				}

				if (HasComponents<Components::Entity::InactiveGloballyFlag>()) {
					if (HasComponents<Components::Entity::Hierarchy>()) {
						const auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
						const entt::entity parent = hierarchy.m_Parent;
						const entt::entity firstChild = hierarchy.m_FirstChild;
						const entt::registry* registry = m_EntityHandle.registry();
						if (registry->valid(parent)) {
							if (registry->all_of<Components::Entity::InactiveGloballyFlag>(parent)) {
								return;
							}
						}
						if (registry->valid(firstChild)) {
							UpdateInactivityFlagsRecursive(m_EntityHandle.entity(), true);
							return;
						}
					}

					EraseComponents<Components::Entity::InactiveGloballyFlag>();
					return;
				}
				return;
			}

			if (!HasComponents<Components::Entity::InactiveLocallyFlag>()) {
				AddComponent<Components::Entity::InactiveLocallyFlag>();
			}

			if (!HasComponents<Components::Entity::InactiveGloballyFlag>()) {
				if (HasComponents<Components::Entity::Hierarchy>()) {
					const auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
					const entt::entity firstChild = hierarchy.m_FirstChild;
					const entt::registry* registry = m_EntityHandle.registry();
					if (registry->valid(firstChild)) {
						UpdateInactivityFlagsRecursive(m_EntityHandle.entity(), false);
						return;
					}
				}

				AddComponent<Components::Entity::InactiveGloballyFlag>();
			}
		}

		bool Entity::IsActiveLocally() const {
			return !HasComponents<Components::Entity::InactiveLocallyFlag>();
		}

		bool Entity::IsActiveGlobally() const {
			return !HasComponents<Components::Entity::InactiveGloballyFlag>();
		}

		std::string Entity::GetDebugData(bool showUUID) const {
			if (showUUID) {
				if (CORI_CORE_CHECK(HasComponents<Components::Entity::UUID>(), "Calling GetDebugData with showUUID=true but entity '{}' doesn't have a UUID component, it will not be shown.", GetDebugData())) {
					showUUID = false;
				}
			}
			if (IsValid()) {
				return std::format("(Entity ID: '{}', Version: '{}', Name: '{}'{}", GetID(), GetVersion(), GetName(), showUUID ? std::format(", UUID: '{}')", GetComponents<Components::Entity::UUID>().m_UUID.GetSerializationString()): ")");
			}
			return "GetDebugData: Error: This Entity is no longer valid.";
		}

		// ReSharper disable once CppParameterMayBeConst
		std::expected<void, Core::CoriError<>> Entity::SetParent(Entity parent) {
			return Systems::Hierarchy::SetParent(*this, parent);
		}

		std::expected<Entity, Core::CoriError<>> Entity::GetParent() const {
			return Systems::Hierarchy::GetParent(*this);
		}

		std::expected<std::vector<Entity>, Core::CoriError<>> Entity::GetSiblings() const {
			return Systems::Hierarchy::GetSiblings(*this);
		}

		std::expected<std::vector<Entity>, Core::CoriError<>> Entity::GetChildren() const {
			return Systems::Hierarchy::GetChildren(*this);
		}

		std::expected<Entity, Core::CoriError<>> Entity::FindChildByName(const char* name) const {
			return Systems::Hierarchy::FindChildByName(*this, name);
		}

		std::expected<Entity, Core::CoriError<>> Entity::FindChildByName(const std::string_view name) const {
			return Systems::Hierarchy::FindChildByName(*this, name);
		}

		std::expected<Entity, Core::CoriError<>> Entity::FindChildByName(const std::string& name) const {
			return Systems::Hierarchy::FindChildByName(*this, name);
		}

		void Entity::DestroyChildren() {
			Systems::Hierarchy::DestroyChildren(*this);
		}

		void Entity::PrintHierarchy() const {
			Systems::Hierarchy::PrintHierarchy(*this);
		}

		std::string_view Entity::GetName() const {
			static constexpr char empty[] = "";
			if (HasComponents<Components::Entity::Name>()) {
				return GetComponents<Components::Entity::Name>().m_Name;
			}

			return {empty};
		}
		void Entity::SetName(const std::string& name) {
			auto& nameComponent = GetOrAddComponent<Components::Entity::Name>();
			if (nameComponent.m_Name == name) {
				return;
			}

			if (HasComponents<Components::Entity::Hierarchy>()) {
				const auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
				entt::registry* registry = m_EntityHandle.registry();
				if (registry->valid(hierarchy.m_Parent)) {
					const entt::entity parent = hierarchy.m_Parent;
					auto& cache = registry->get<Components::Entity::ChildCache>(parent);
					cache.m_Children.erase(nameComponent.m_Name);
					cache.m_Children.emplace(name, m_EntityHandle.entity());
				}
			}

			nameComponent.m_Name = name;
		}


		void Entity::UnlinkFromParent() {
			Systems::Hierarchy::UnlinkFromParent(*this);
		}

		void Entity::DrawHierarchyRecursive(const Entity& entity, const std::string& prefix, const bool isLast) {
			Systems::Hierarchy::DrawHierarchyRecursive(entity, prefix, isLast);
		}

		void Entity::UpdateInactivityFlagsRecursive(entt::entity parent, const bool parentIsActive) {
			CORI_PROFILE_FUNCTION();
			entt::registry* registry = m_EntityHandle.registry();

			const bool finalEffectiveState = parentIsActive && !registry->all_of<Components::Entity::InactiveLocallyFlag>(parent);

			if (finalEffectiveState) {
				registry->remove<Components::Entity::InactiveGloballyFlag>(parent);
			} else {
				registry->emplace_or_replace<Components::Entity::InactiveGloballyFlag>(parent);
			}

			const auto& hierarchy = registry->get<Components::Entity::Hierarchy>(parent);
			entt::entity currentChild = hierarchy.m_FirstChild;
			while (registry->valid(currentChild)) {
				UpdateInactivityFlagsRecursive(currentChild, finalEffectiveState);
				currentChild = registry->get<Components::Entity::Hierarchy>(currentChild).m_NextSibling;
			}
		}
	}
}