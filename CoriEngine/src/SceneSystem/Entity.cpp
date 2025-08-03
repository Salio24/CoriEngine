#include "Entity.hpp"
#include "Scene.hpp"

namespace Cori {
/*
	enum class SetParentErrorCode {
		MissingTagComponent,
		InvalidTagComponent
	};

	struct SetParentError {
		SetParentErrorCode m_Code;
		std::string m_Message;

		operator std::string() const {
			switch (m_Code) {
				case SetParentErrorCode::MissingTagComponent:
				return std::string{"MissingTagComponent error, setting a parent for the entity and thus making it a child is not allowed if entity doesn't have Cori::Components::Entity::TagComponent. Message: "} + m_Message;
				case SetParentErrorCode::InvalidTagComponent:
				return std::string{"InvalidTagComponent error, Cori::Components::Entity::TagComponent of the entity is invalid. You likely passed the default initialized HashedTag64 to TagComponent. Message: "} + m_Message;
				default:
				return std::string{"Unknown error. Message: "} + m_Message;
			}
		}
	};
*/

	void Entity::SetActive(bool state) {
		if (state) {
			if (HasComponents<Components::Entity::InactiveLocallyFlag>()) {
				EraseComponents<Components::Entity::InactiveLocallyFlag>();
			}

			if (HasComponents<Components::Entity::InactiveGloballyFlag>()) {
				if (HasComponents<Components::Entity::Hierarchy>()) {
					auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
					entt::entity parentID = hierarchy.m_Parent;
					entt::entity firstChildID = hierarchy.m_FirstChild;
					entt::registry* registry = m_EntityHandle.registry();
					if (registry->valid(parentID)) {
						if (registry->all_of<Components::Entity::InactiveGloballyFlag>(parentID)) {
							return;
						}
					}
					if (registry->valid(firstChildID)) {
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
				auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
				entt::entity firstChildID = hierarchy.m_FirstChild;
				entt::registry* registry = m_EntityHandle.registry();
				if (registry->valid(firstChildID)) {
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

	std::string Entity::GetDebugData(bool showUUID) const {
		return "(Entity ID: '" + std::to_string(GetID()) + "', Version: '" + std::to_string(GetVersion()) + "', Name: '" + GetComponents<Components::Entity::Name>().m_Name + "', Tag: '" + std::string(GetComponents<Components::Entity::Tag>().m_Tag.m_DebugName) + (showUUID ? "', UUID: '" + GetComponents<Components::Entity::UUID>().m_UUID.GetSerializationString() + "')" : "')");
	}

	std::expected<void, const char*> Entity::SetParent(Entity parent) {
		GetOrAddComponent<Components::Entity::Hierarchy>();

		UnlinkFromParent();

		if (parent.IsValid()) {
			LinkToParent(parent);
		}
		return {};
	}

	std::expected<Entity, const char*> Entity::GetParent() const {
		if (!HasComponents<Components::Entity::Hierarchy>()) { return std::unexpected("Error: Entity doesn't have HierarchyComponent, and thus doesn't have a parent."); }
		auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
		if (hierarchy.m_Parent == entt::null) { return std::unexpected("Error: Entity doesn't have a parent."); }
		return Entity{entt::handle{*m_EntityHandle.registry(), hierarchy.m_Parent}};
	}

	// add get siblings

	std::expected<std::vector<Entity>, const char*> Entity::GetSiblings() const {
		if (!HasComponents<Components::Entity::Hierarchy>()) { return std::unexpected("Error: Entity doesn't have HierarchyComponent, and thus doesn't have any siblings."); }

		auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
		entt::registry* registry = m_EntityHandle.registry();
		if (!registry->valid(hierarchy.m_Parent)) {
			return std::unexpected("Error: Entity doesn't have a parent, and thus doesn't have any siblings.");
		}

		Entity parent = entt::handle{*registry, hierarchy.m_Parent};
		auto siblings = parent.GetChildren();
		if (siblings) {
			return siblings.value();
		}
		return std::unexpected("Unknown error occurred.");
	}

	std::expected<std::vector<Entity>, const char*> Entity::GetChildren() const {
		std::vector<Entity> children;
		if (!HasComponents<Components::Entity::Hierarchy>()) { return std::unexpected("Error: Entity doesn't have HierarchyComponent, and thus doesn't have any children."); }

		entt::registry* registry = m_EntityHandle.registry();
		auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
		entt::entity currentChildID = hierarchy.m_FirstChild;

		while (registry->valid(currentChildID)) {
			children.emplace_back(entt::handle{*registry, currentChildID});
			currentChildID = registry->get<Components::Entity::Hierarchy>(currentChildID).m_NextSibling;
		}
		return children;
	}

	std::expected<Entity, const char*> Entity::FindChildByName(const std::string& name) const {

		auto& cache = GetComponents<Components::Entity::ChildCache>();

		if (cache.m_Children.contains(name)) {
			entt::entity childID = cache.m_Children.at(name);
			return Entity{{*m_EntityHandle.registry(), childID}};
		}
		return std::unexpected("Error: No children found with the specified tag.");
	}

	void Entity::PrintHierarchy() {
		CORI_CORE_DEBUG_TAGGED({"World", "Entity", "Hierarchy"}, "Printing full hierarchy tree of '{}'", GetComponents<Components::Entity::Name>().m_Name);
		entt::entity rootID = GetRawEntity();
		entt::entity currentParentID = GetComponents<Components::Entity::Hierarchy>().m_Parent;
		while (m_EntityHandle.registry()->valid(currentParentID)) {
			rootID = currentParentID;
			currentParentID = m_EntityHandle.registry()->get<Components::Entity::Hierarchy>(rootID).m_Parent;
		}

		Entity root{entt::handle{*m_EntityHandle.registry(), rootID}};

		std::string rootName = root.GetComponents<Components::Entity::Name>().m_Name;
		CORI_CORE_DEBUG_TAGGED({"World", "Entity", "Hierarchy"}, "Hierarchy top level root/parent '{}'", rootName);

		CORI_CORE_TRACE_TAGGED({"World", "Entity", "Hierarchy"}, "{}", rootName);
		auto children = root.GetChildren();
		if (children) {
			for (size_t i = 0; i < children.value().size(); ++i) {
				auto& child = children.value()[i];
				bool isLastChild = (i == children.value().size() - 1);
				DrawHierarchyRecursive(child, "  ", isLastChild);
			}
		}
		CORI_CORE_DEBUG_TAGGED({"World", "Entity", "Hierarchy"}, "Finished");
	}

	std::string Entity::GetName() {
		return GetComponents<Components::Entity::Name>().m_Name;
	}
	void Entity::SetName(const std::string& name) {
		auto& nameComponent = GetComponents<Components::Entity::Name>();
		if (nameComponent.m_Name == name) {
			return;
		}

		if (HasComponents<Components::Entity::Hierarchy>()) {
			auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
			entt::registry* registry = m_EntityHandle.registry();
			if (registry->valid(hierarchy.m_Parent)) {
				entt::entity parentID = hierarchy.m_Parent;
				auto& cache = registry->get<Components::Entity::ChildCache>(parentID);
				cache.m_Children.erase(nameComponent.m_Name);
				cache.m_Children.emplace(name, m_EntityHandle.entity());
			}
		}
	}


	void Entity::UnlinkFromParent() {
		entt::registry* registry = m_EntityHandle.registry();
		auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();
		entt::entity parentID = hierarchy.m_Parent;
		if (!registry->valid(parentID)) { return; }


		auto& cache = registry->get<Components::Entity::ChildCache>(parentID);
		cache.m_Children.erase(GetComponents<Components::Entity::Name>().m_Name);

		auto& parentHierarchy = registry->get<Components::Entity::Hierarchy>(parentID);
		entt::entity previousSiblingID = parentHierarchy.m_PreviousSibling;
		entt::entity nextSiblingID = parentHierarchy.m_NextSibling;

		if (parentHierarchy.m_FirstChild == m_EntityHandle.entity()) {
			parentHierarchy.m_FirstChild = nextSiblingID;
		} else {
			registry->get<Components::Entity::Hierarchy>(previousSiblingID).m_NextSibling = nextSiblingID;
		}

		if (registry->valid(nextSiblingID)) {
			registry->get<Components::Entity::Hierarchy>(nextSiblingID).m_PreviousSibling = previousSiblingID;
		}

		hierarchy.m_Parent = entt::null;
		parentHierarchy.m_NextSibling = entt::null;
		parentHierarchy.m_PreviousSibling = entt::null;
		UpdateInactivityFlagsRecursive(m_EntityHandle.entity(), true);
	}

	void Entity::LinkToParent(Entity parent) {
		entt::registry* registry = m_EntityHandle.registry();
		auto& hierarchy = GetComponents<Components::Entity::Hierarchy>();

		hierarchy.m_Parent = parent.GetRawEntity();

		auto& cache = parent.GetOrAddComponent<Components::Entity::ChildCache>();
		cache.m_Children.emplace(GetComponents<Components::Entity::Name>().m_Name, m_EntityHandle.entity());

		auto& parentHierarchy = parent.GetOrAddComponent<Components::Entity::Hierarchy>();
		entt::entity firstChildID = parentHierarchy.m_FirstChild;

		if (registry->valid(firstChildID)) {
			registry->get<Components::Entity::Hierarchy>(firstChildID).m_PreviousSibling = m_EntityHandle.entity();
			hierarchy.m_NextSibling = firstChildID;
		}

		parentHierarchy.m_FirstChild = m_EntityHandle.entity();
		UpdateInactivityFlagsRecursive(m_EntityHandle.entity(), parent.IsActiveGlobally());
	}

	void Entity::DrawHierarchyRecursive(Entity entity, const std::string& prefix, bool isLast) {
		std::string entityName = entity.GetComponents<Components::Entity::Name>().m_Name;

		CORI_CORE_TRACE_TAGGED({"World", "Entity", "Hierarchy"}, "{}{}{}", prefix, isLast ? "└─" : "├─", entityName);

		std::string childPrefix = prefix + (isLast ? "    " : "│   ");

		auto children = entity.GetChildren();
		if (children) {
			for (size_t i = 0; i < children.value().size(); ++i) {
				auto& child = children.value()[i];
				bool isLastChild = (i == children.value().size() - 1);
				DrawHierarchyRecursive(child, childPrefix, isLastChild);
			}
		}
	}

	void Entity::UpdateInactivityFlagsRecursive(entt::entity parent, bool parentIsActive) {
		entt::registry* registry = m_EntityHandle.registry();

		bool finalEffectiveState = parentIsActive && !registry->all_of<Components::Entity::InactiveLocallyFlag>(parent);

		if (finalEffectiveState) {
			registry->remove<Components::Entity::InactiveGloballyFlag>(parent);
		} else {
			registry->emplace<Components::Entity::InactiveLocallyFlag>(parent);
		}

		auto& hierarchy = registry->get<Components::Entity::Hierarchy>(parent);
		entt::entity currentChildID = hierarchy.m_FirstChild;
		while (registry->valid(currentChildID)) {
			UpdateInactivityFlagsRecursive(currentChildID, finalEffectiveState);
			currentChildID = registry->get<Components::Entity::Hierarchy>(currentChildID).m_NextSibling;
		}
	}
}