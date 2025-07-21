#include "Entity.hpp"
#include "Scene.hpp"

namespace Cori {
	//for internal use ONLY
	static Scene* m_ViewScene;
	// ewwwww who wrote that? was i drunk?

	Entity::Entity(const entt::entity& entity) {
		m_EntityHandle = entt::handle{ m_ViewScene->m_Registry, entity };
	}
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

	std::expected<void, const char*> Entity::SetParent(Entity parent) {
		GetOrAddComponent<Components::Entity::HierarchyComponent>();

		if (!HasComponents<Components::Entity::TagComponent>()) {
			return std::unexpected("Error: Setting a parent for the entity and thus making it a child is not allowed if entity doesn't have Cori::Components::Entity::TagComponent.");
		}
		if (GetComponents<Components::Entity::TagComponent>().m_Tag.m_Hash == 0) {
			return std::unexpected("Error: Cori::Components::Entity::TagComponent of the entity is invalid. (m_Tag.m_Hash == 0)");
		}

		UnlinkFromParent();

		if (parent.IsValid()) {
			LinkToParent(parent);
		}
		return {};
	}

	std::expected<Entity, const char*> Entity::GetParent() const {
		if (!HasComponents<Components::Entity::HierarchyComponent>()) { return std::unexpected("Error: Entity doesn't have HierarchyComponent, and thus doesn't have a parent."); }
		const auto& hierarchy = GetComponents<Components::Entity::HierarchyComponent>();
		if (hierarchy.m_Parent == entt::null) { return std::unexpected("Error: Entity doesn't have a parent."); }
		return Entity{entt::handle{*m_EntityHandle.registry(), hierarchy.m_Parent}};
	}

	std::expected<std::vector<Entity>, const char*> Entity::GetChildren() const {
		std::vector<Entity> children;
		if (!HasComponents<Components::Entity::HierarchyComponent>()) { return std::unexpected("Error: Entity doesn't have HierarchyComponent, and thus doesn't have any children."); }

		entt::registry* registry = m_EntityHandle.registry();
		const auto& hierarchy = GetComponents<Components::Entity::HierarchyComponent>();
		entt::entity currentChildID = hierarchy.m_FirstChild;

		while (registry->valid(currentChildID)) {
			children.emplace_back(entt::handle{*registry, currentChildID});
			currentChildID = registry->get<Components::Entity::HierarchyComponent>(currentChildID).m_NextSibling;
		}
		return children;
	}

	// change to tag not stringhash
	std::expected<Entity, const char*> Entity::FindChildByName(Utils::StringHash64 tag) const {

		const auto& cache = GetComponents<Components::Entity::ChildCacheComponent>();

		if (cache.m_Children.contains(tag)) {
			entt::entity childID = cache.m_Children.at(tag);
			return Entity{{*m_EntityHandle.registry(), childID}};
		}
		return std::unexpected("Error: No children found with the specified tag.");
	}

	void Entity::PrintHierarchy() {
#ifdef DEBUG_BUILD
		CORI_CORE_DEBUG_TAGGED({"ECS", "Entity", "Hirarchy"}, "Printing full hierarchy tree of '{}'", GetComponents<Components::Entity::TagComponent>().m_Tag.m_DebugName);
		entt::entity rootID = GetHandle();
		entt::entity currentParentID = GetComponents<Components::Entity::HierarchyComponent>().m_Parent;
		while (m_EntityHandle.registry()->valid(currentParentID)) {
			rootID = currentParentID;
			currentParentID = m_EntityHandle.registry()->get<Components::Entity::HierarchyComponent>(rootID).m_Parent;
		}

		Entity root{entt::handle{*m_EntityHandle.registry(), rootID}};

		std::string rootName = root.GetComponents<Components::Entity::TagComponent>().m_Tag.m_DebugName;
		CORI_CORE_DEBUG_TAGGED({"ECS", "Entity", "Hirarchy"}, "Hierarchy top level root/parent '{}'", rootName);

		CORI_CORE_TRACE_TAGGED({"ECS", "Entity", "Hirarchy"}, "{}", rootName);
		auto children = root.GetChildren();
		if (children) {
			for (size_t i = 0; i < children.value().size(); ++i) {
				const auto& child = children.value()[i];
				bool isLastChild = (i == children.value().size() - 1);
				DrawHierarchyRecursive(child, "  ", isLastChild);
			}
		}
		CORI_CORE_DEBUG_TAGGED({"ECS", "Entity", "Hirarchy"}, "Finished");
#endif
	}

	void Entity::UnlinkFromParent() {
		entt::registry* registry = m_EntityHandle.registry();
		auto& hierarchy = GetComponents<Components::Entity::HierarchyComponent>();
		entt::entity parentID = hierarchy.m_Parent;
		if (!registry->valid(parentID)) { return; }


		auto& cache = registry->get<Components::Entity::ChildCacheComponent>(parentID);
		cache.m_Children.erase(GetComponents<Components::Entity::TagComponent>().m_Tag.m_Hash);

		auto& parentHierarchy = registry->get<Components::Entity::HierarchyComponent>(parentID);
		entt::entity previousSiblingID = parentHierarchy.m_PreviousSibling;
		entt::entity nextSiblingID = parentHierarchy.m_NextSibling;

		if (parentHierarchy.m_FirstChild == m_EntityHandle.entity()) {
			parentHierarchy.m_FirstChild = nextSiblingID;
		} else {
			registry->get<Components::Entity::HierarchyComponent>(previousSiblingID).m_NextSibling = nextSiblingID;
		}

		if (registry->valid(nextSiblingID)) {
			registry->get<Components::Entity::HierarchyComponent>(nextSiblingID).m_PreviousSibling = previousSiblingID;
		}

		hierarchy.m_Parent = entt::null;
		parentHierarchy.m_NextSibling = entt::null;
		parentHierarchy.m_PreviousSibling = entt::null;

	}
	void Entity::LinkToParent(Entity parent) {
		entt::registry* registry = m_EntityHandle.registry();
		auto& hierarchy = GetComponents<Components::Entity::HierarchyComponent>();

		hierarchy.m_Parent = parent.GetHandle();

		auto& cache = parent.GetOrAddComponent<Components::Entity::ChildCacheComponent>();
		cache.m_Children.insert({GetComponents<Components::Entity::TagComponent>().m_Tag.m_Hash, m_EntityHandle.entity()});

		auto& parentHierarchy = parent.GetOrAddComponent<Components::Entity::HierarchyComponent>();
		entt::entity firstChildID = parentHierarchy.m_FirstChild;

		if (registry->valid(firstChildID)) {
			registry->get<Components::Entity::HierarchyComponent>(firstChildID).m_PreviousSibling = m_EntityHandle.entity();
			hierarchy.m_NextSibling = firstChildID;
		}

		parentHierarchy.m_FirstChild = m_EntityHandle.entity();
	}

	void Entity::DrawHierarchyRecursive(Entity entity, const std::string& prefix, bool isLast) {
#ifdef DEBUG_BUILD
		std::string entityName = entity.GetComponents<Components::Entity::TagComponent>().m_Tag.m_DebugName;

		CORI_CORE_TRACE_TAGGED({"ECS", "Entity", "Hirarchy"}, "{}{}{}", prefix, isLast ? "└─" : "├─", entityName);

		std::string childPrefix = prefix + (isLast ? "    " : "│   ");

		auto children = entity.GetChildren();
		if (children) {
			for (size_t i = 0; i < children.value().size(); ++i) {
				const auto& child = children.value()[i];
				bool isLastChild = (i == children.value().size() - 1);
				DrawHierarchyRecursive(child, childPrefix, isLastChild);
			}
		}
#endif
	}

	void Entity::SetViewScene(Scene* ptr) {
		m_ViewScene = ptr;
	}




}