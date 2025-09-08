#include "Entity.hpp"
#include "Scene.hpp"

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
						EraseComponents<Components::Entity::InactiveGloballyFlag>();
						return;
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
					AddComponent<Components::Entity::InactiveGloballyFlag>();
					return;
				}
				AddComponent<Components::Entity::InactiveGloballyFlag>();
			}
		}

		bool Entity::IsActiveLocally() const {
			if (HasComponents<Components::Entity::InactiveLocallyFlag>()) { return false; }
			return true;
		}

		bool Entity::IsActiveGlobally() const {
			if (HasComponents<Components::Entity::InactiveGloballyFlag>()) { return false; }
			return true;
		}

		std::string Entity::GetDebugData(const bool showUUID) const {
			if (IsValid()) {
				return "(Entity ID: '" + std::to_string(GetID()) + "', Version: '" + std::to_string(GetVersion()) + "', Name: '" + GetComponents<Components::Entity::Name>().m_Name + "', Tag: '" + std::string(GetComponents<Components::Entity::Tag>().m_Tag.m_DebugName) + (showUUID ? "', UUID: '" + GetComponents<Components::Entity::UUID>().m_UUID.GetSerializationString() + "')" : "')");
			}
			return "GetDebugData: Error: This Entity is no longer valid.";
		}

		// ReSharper disable once CppParameterMayBeConst
		std::expected<void, Core::CoriError<>> Entity::SetParent(Entity parent) {
			UnlinkFromParent();

			if (parent.IsValid()) {
				return LinkToParent(parent);
			}
			return {};
		}

		std::expected<Entity, Core::CoriError<>> Entity::GetParent() const {
			if (!HasComponents<Components::Entity::Hierarchy>()) {
				return std::unexpected(Core::CoriError("Entity doesn't have HierarchyComponent, and thus doesn't have a parent."));
			}

			auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
			if (hierarchy.m_Parent == entt::null) {
				return std::unexpected(Core::CoriError("Entity doesn't have a parent."));
			}

			return Entity{ entt::handle{ *m_EntityHandle.registry(), hierarchy.m_Parent } };
		}

		std::expected<std::vector<Entity>, Core::CoriError<>> Entity::GetSiblings() const {
			if (!HasComponents<Components::Entity::Hierarchy>()) { return std::unexpected(Core::CoriError("Entity doesn't have HierarchyComponent, and thus doesn't have any siblings.")); }

			auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
			entt::registry* registry = m_EntityHandle.registry();
			if (!registry->valid(hierarchy.m_Parent)) {
				return std::unexpected(Core::CoriError("Entity doesn't have a parent, and thus doesn't have any siblings."));
			}

			const Entity parent = entt::handle{ *registry, hierarchy.m_Parent };
			auto siblings = parent.GetChildren();
			if (siblings) {
				return siblings.value();
			}

			return std::unexpected(Core::CoriError("Unknown error occurred."));
		}

		std::expected<std::vector<Entity>, Core::CoriError<>> Entity::GetChildren() const {
			std::vector<Entity> children;
			if (!HasComponents<Components::Entity::Hierarchy>()) { return std::unexpected(Core::CoriError("Entity doesn't have HierarchyComponent, and thus doesn't have any children.")); }

			entt::registry* registry = m_EntityHandle.registry();
			auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
			entt::entity currentChild = hierarchy.m_FirstChild;

			while (registry->valid(currentChild)) {
				children.emplace_back(entt::handle{*registry, currentChild});
				currentChild = registry->get<Components::Entity::Hierarchy>(currentChild).m_NextSibling;
			}
			return children;
		}

		std::expected<Entity, Core::CoriError<>> Entity::FindChildByName(const std::string& name) const {
			auto& cache = GetComponents<Components::Entity::ChildCache>();

			if (cache.m_Children.contains(name)) {
				entt::entity child = cache.m_Children.at(name);
				return Entity{ { *m_EntityHandle.registry(), child } };
			}
			return std::unexpected(Core::CoriError("No children found with the specified name."));
		}

		void Entity::PrintHierarchy() const {
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Printing full hierarchy tree of '{}'", GetComponents<Components::Entity::Name>().m_Name);
			entt::entity root = GetRawEntity();
			entt::entity currentParent = GetComponents<Components::Entity::Hierarchy>().m_Parent;
			while (m_EntityHandle.registry()->valid(currentParent)) {
				root = currentParent;
				currentParent = m_EntityHandle.registry()->get<Components::Entity::Hierarchy>(root).m_Parent;
			}

			Entity rootHandle{ entt::handle{ *m_EntityHandle.registry(), root } };

			const std::string rootName = rootHandle.GetComponents<Components::Entity::Name>().m_Name;
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Hierarchy top level root/parent '{}'", rootName);

			CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "{}", rootName);
			const auto children = rootHandle.GetChildren();
			if (children) {
				for (size_t i = 0; i < children.value().size(); ++i) {
					const auto& child = children.value()[i];
					const bool isLastChild = i == children.value().size() - 1;
					DrawHierarchyRecursive(child, "  ", isLastChild);
				}
			}
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Finished");
		}

		std::string Entity::GetName() const {
			return GetComponents<Components::Entity::Name>().m_Name;
		}
		void Entity::SetName(const std::string& name) {
			auto& nameComponent = GetComponents<Components::Entity::Name>();
			if (nameComponent.m_Name == name) {
				return;
			}
			nameComponent.m_Name = name;

			if (HasComponents<Components::Entity::Hierarchy>()) {
				const auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
				entt::registry* registry = m_EntityHandle.registry();
				if (registry->valid(hierarchy.m_Parent)) {
					// ReSharper disable once CppLocalVariableMayBeConst
					entt::entity parent = hierarchy.m_Parent;
					auto& cache = registry->get<Components::Entity::ChildCache>(parent);
					cache.m_Children.erase(nameComponent.m_Name);
					cache.m_Children.emplace(name, m_EntityHandle.entity());
				}
			}
		}


		void Entity::UnlinkFromParent() {
			entt::registry* registry = m_EntityHandle.registry();
			auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
			const entt::entity parent = hierarchy.m_Parent;
			if (!registry->valid(parent)) { return; }


			auto& cache = registry->get<Components::Entity::ChildCache>(parent);
			cache.m_Children.erase(GetComponents<Components::Entity::Name>().m_Name);

			auto& parentHierarchy = registry->get<Components::Entity::Hierarchy>(parent);
			// ReSharper disable once CppLocalVariableMayBeConst
			entt::entity previousSibling = parentHierarchy.m_PreviousSibling;
			// ReSharper disable once CppLocalVariableMayBeConst
			entt::entity nextSibling = parentHierarchy.m_NextSibling;

			if (parentHierarchy.m_FirstChild == m_EntityHandle.entity()) {
				parentHierarchy.m_FirstChild = nextSibling;
			} else {
				registry->get<Components::Entity::Hierarchy>(previousSibling).m_NextSibling = nextSibling;
			}

			if (registry->valid(nextSibling)) {
				registry->get<Components::Entity::Hierarchy>(nextSibling).m_PreviousSibling = previousSibling;
			}

			hierarchy.m_Parent = entt::null;
			parentHierarchy.m_NextSibling = entt::null;
			parentHierarchy.m_PreviousSibling = entt::null;
			UpdateInactivityFlagsRecursive(m_EntityHandle.entity(), true);
		}

		std::expected<void, Core::CoriError<>> Entity::LinkToParent(Entity parent) {
			entt::registry* registry = m_EntityHandle.registry();
			auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();

			hierarchy.m_Parent = parent.GetRawEntity();

			auto& cache = parent.GetOrAddComponent<Components::Entity::ChildCache>();
			const auto& nameComp = GetComponents<Components::Entity::Name>();
			if (cache.m_Children.contains(nameComp.m_Name)) {
				return std::unexpected(Core::CoriError("A parent entity can't have 2 children with the same name."));
			}
			cache.m_Children.emplace(nameComp.m_Name, m_EntityHandle.entity());

			auto& parentHierarchy = parent.GetComponents<Components::Entity::Hierarchy>();
			// ReSharper disable once CppLocalVariableMayBeConst
			entt::entity firstChild = parentHierarchy.m_FirstChild;

			if (registry->valid(firstChild)) {
				registry->get<Components::Entity::Hierarchy>(firstChild).m_PreviousSibling = m_EntityHandle.entity();
				hierarchy.m_NextSibling = firstChild;
			}

			parentHierarchy.m_FirstChild = m_EntityHandle.entity();
			UpdateInactivityFlagsRecursive(m_EntityHandle.entity(), parent.IsActiveGlobally());
			return {};
		}

		void Entity::DrawHierarchyRecursive(const Entity& entity, const std::string& prefix, const bool isLast) {
			std::string entityName = entity.GetComponents<Components::Entity::Name>().m_Name;

			CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "{}{}{}", prefix, isLast ? "└─" : "├─", entityName);

			const std::string childPrefix = prefix + (isLast ? "    " : "│   ");

			const auto children = entity.GetChildren();
			if (children) {
				for (size_t i = 0; i < children.value().size(); ++i) {
					const auto& child = children.value()[i];
					const bool isLastChild = i == children.value().size() - 1;
					DrawHierarchyRecursive(child, childPrefix, isLastChild);
				}
			}
		}

		// ReSharper disable once CppParameterMayBeConst
		void Entity::UpdateInactivityFlagsRecursive(entt::entity parent, const bool parentIsActive) {
			entt::registry* registry = m_EntityHandle.registry();

			const bool finalEffectiveState = parentIsActive && !registry->all_of<Components::Entity::InactiveLocallyFlag>(parent);

			if (finalEffectiveState) {
				registry->remove<Components::Entity::InactiveGloballyFlag>(parent);
			} else {
				registry->emplace<Components::Entity::InactiveLocallyFlag>(parent);
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