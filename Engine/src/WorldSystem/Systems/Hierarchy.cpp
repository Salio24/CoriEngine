#include "Hierarchy.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			bool Hierarchy::Create() {
				m_Owner.GetRegistry().on_destroy<Components::Entity::Hierarchy>().connect<&Hierarchy::OnHierarchyComponentDestroyed>(this);
				return true;
			}

			std::expected<void, Core::CoriError<>> Hierarchy::SetParent(Entity subject, Entity parent) {
				UnlinkFromParent(subject);

				if (parent.IsValid()) {
					return LinkToParent(subject, parent);
				}
				return {};
			}

			std::expected<void, Core::CoriError<>> Hierarchy::LinkToParent(Entity subject, Entity parent) {
				entt::registry* registry = subject.GetRawHandle().registry();
				if (!subject.HasComponents<Components::Entity::Name>() || !parent.HasComponents<Components::Entity::Name>()) {
					return std::unexpected(Core::CoriError("Linking 2 entities is only allowed when both of them have a name."));
				}

				auto& hierarchy = subject.GetOrAddComponent<Components::Entity::Hierarchy>();

				hierarchy.m_Parent = parent.GetRawEntity();

				auto& cache = parent.GetOrAddComponent<Components::Entity::ChildCache>();
				const auto& name = subject.GetName();
				if (cache.m_Children.contains(name)) {
					return std::unexpected(Core::CoriError("A parent entity can't have 2 children with the same name."));
				}
				cache.m_Children.emplace(name, subject.GetRawHandle().entity());

				auto& parentHierarchy = parent.GetOrAddComponent<Components::Entity::Hierarchy>();
				const entt::entity firstChild = parentHierarchy.m_FirstChild;

				if (registry->valid(firstChild)) {
					registry->get<Components::Entity::Hierarchy>(firstChild).m_PreviousSibling = subject.GetRawHandle().entity();
					hierarchy.m_NextSibling = firstChild;
				}

				parentHierarchy.m_FirstChild = subject.GetRawHandle().entity();
				subject.UpdateInactivityFlagsRecursive(subject.GetRawHandle().entity(), parent.IsActiveGlobally());
				return {};
			}

			void Hierarchy::UnlinkFromParent(Entity subject) {
				entt::registry* registry = subject.GetRawHandle().registry();
				if (!subject.HasComponents<Components::Entity::Hierarchy, Components::Entity::Name>()) {
					return;
				}
				auto& hierarchy = subject.GetComponents<Components::Entity::Hierarchy>();
				const entt::entity parent = hierarchy.m_Parent;
				if (!registry->valid(parent)) {
					return;
				}

				if (!registry->all_of<Components::Entity::ChildCache, Components::Entity::Hierarchy>(parent)) {
					return;
				}

				auto& cache = registry->get<Components::Entity::ChildCache>(parent);
				cache.m_Children.erase(std::string(subject.GetName()));

				auto& parentHierarchy = registry->get<Components::Entity::Hierarchy>(parent);
				const entt::entity previousSibling = hierarchy.m_PreviousSibling;
				const entt::entity nextSibling = hierarchy.m_NextSibling;

				if (parentHierarchy.m_FirstChild == subject.GetRawHandle().entity()) {
					parentHierarchy.m_FirstChild = nextSibling;
				} else {
					if (registry->valid(previousSibling)) {
						registry->get<Components::Entity::Hierarchy>(previousSibling).m_NextSibling = nextSibling;
					}
				}

				if (registry->valid(nextSibling)) {
					registry->get<Components::Entity::Hierarchy>(nextSibling).m_PreviousSibling = previousSibling;
				}

				hierarchy.m_Parent = entt::null;
				hierarchy.m_NextSibling = entt::null;
				hierarchy.m_PreviousSibling = entt::null;
				subject.UpdateInactivityFlagsRecursive(subject.GetRawHandle().entity(), true);
			}

			std::expected<std::vector<Entity>, Core::CoriError<>> Hierarchy::GetSiblings(Entity subject) {
				if (!subject.HasComponents<Components::Entity::Hierarchy>()) {
					return std::unexpected(Core::CoriError("Entity doesn't have hierarchy component, and thus doesn't have any siblings."));
				}
				auto& hierarchy = subject.GetComponents<Components::Entity::Hierarchy>();
				entt::registry* registry = subject.GetRawHandle().registry();
				if (!registry->valid(hierarchy.m_Parent)) {
					return std::unexpected(Core::CoriError("Entity doesn't have a parent, and thus doesn't have any siblings."));
				}

				const Entity parent = entt::handle{ *registry, hierarchy.m_Parent };
				auto siblings = parent.GetChildren();
				if (siblings) {
					return siblings.value();
				}

				return std::unexpected(siblings.error());
			}

			std::expected<std::vector<Entity>, Core::CoriError<>> Hierarchy::GetChildren(Entity subject) {
				std::vector<Entity> children;

				if (!subject.HasComponents<Components::Entity::ChildCache>()) {
					return std::unexpected(Core::CoriError("Entity doesn't have any children."));
				}

				entt::registry* registry = subject.GetRawHandle().registry();
				auto& childCache = subject.GetComponents<Components::Entity::ChildCache>();

				for (const auto& child : childCache.m_Children | std::views::values) {
					children.emplace_back(entt::handle{*registry, child});
				}

				return children;
			}

			std::expected<Entity, Core::CoriError<>> Hierarchy::GetParent(Entity subject) {
				if (!subject.HasComponents<Components::Entity::Hierarchy>()) {
					return std::unexpected(Core::CoriError("Entity doesn't have HierarchyComponent, and thus doesn't have a parent."));
				}

				auto& hierarchy = subject.GetComponents<Components::Entity::Hierarchy>();
				if (hierarchy.m_Parent == entt::null) {
					return std::unexpected(Core::CoriError("Entity doesn't have a parent."));
				}

				return Entity{ entt::handle{ *subject.GetRawHandle().registry(), hierarchy.m_Parent } };
			}

			std::expected<Entity, Core::CoriError<>> Hierarchy::FindChildByName(Entity subject, const char* name) {
				if (!subject.HasComponents<Components::Entity::ChildCache>()) {
					return std::unexpected(Core::CoriError("Entity doesn't have any children."));
				}

				auto& cache = subject.GetComponents<Components::Entity::ChildCache>();

				if (cache.m_Children.contains(name)) {
					entt::entity child = cache.m_Children.find(name)->second;
					return Entity{ { *subject.GetRawHandle().registry(), child } };
				}
				return std::unexpected(Core::CoriError("No children found with the specified name."));
			}

			std::expected<Entity, Core::CoriError<>> Hierarchy::FindChildByName(Entity subject, const std::string_view name) {
				if (!subject.HasComponents<Components::Entity::ChildCache>()) {
					return std::unexpected(Core::CoriError("Entity doesn't have any children."));
				}

				auto& cache = subject.GetComponents<Components::Entity::ChildCache>();

				if (cache.m_Children.contains(name)) {
					entt::entity child = cache.m_Children.find(name)->second;
					return Entity{ { *subject.GetRawHandle().registry(), child } };
				}
				return std::unexpected(Core::CoriError("No children found with the specified name."));
			}

			std::expected<Entity, Core::CoriError<>> Hierarchy::FindChildByName(Entity subject, const std::string& name) {
				if (!subject.HasComponents<Components::Entity::ChildCache>()) {
					return std::unexpected(Core::CoriError("Entity doesn't have any children."));
				}

				auto& cache = subject.GetComponents<Components::Entity::ChildCache>();

				if (cache.m_Children.contains(name)) {
					entt::entity child = cache.m_Children.find(name)->second;
					return Entity{ { *subject.GetRawHandle().registry(), child } };
				}
				return std::unexpected(Core::CoriError("No children found with the specified name."));
			}

			void Hierarchy::DestroyChildren(Entity subject) {
				entt::registry* registry = subject.GetRawHandle().registry();
				const auto& hierarchy = registry->get<Components::Entity::Hierarchy>(subject.GetRawEntity());
				if (registry->all_of<Components::Entity::ChildCache>(subject.GetRawEntity())) {
					auto& cache = registry->get<Components::Entity::ChildCache>(subject.GetRawEntity());
					cache.m_Children.clear();
				}
				entt::entity currentChild = hierarchy.m_FirstChild;
				while (registry->valid(currentChild)) {
					const entt::entity nextChild = registry->get<Components::Entity::Hierarchy>(currentChild).m_NextSibling;
					registry->destroy(currentChild);
					currentChild = nextChild;
				}
			}

			void Hierarchy::PrintHierarchy(Entity subject) {
				if (!subject.HasComponents<Components::Entity::Name>()) {
					return;
				}

				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Printing full hierarchy tree of '{}'", subject.GetName());
				if (subject.HasComponents<Components::Entity::Hierarchy>()) {
					entt::entity root = subject.GetRawEntity();
					entt::entity currentParent = subject.GetComponents<Components::Entity::Hierarchy>().m_Parent;
					while (subject.GetRawHandle().registry()->valid(currentParent)) {
						root = currentParent;
						currentParent = subject.GetRawHandle().registry()->get<Components::Entity::Hierarchy>(root).m_Parent;
					}

					Entity rootHandle{ entt::handle{ *subject.GetRawHandle().registry(), root } };

					const std::string_view rootName = rootHandle.GetName();
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
				} else {
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Hierarchy top level root/parent '{}'", subject.GetName());

					CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "{}", subject.GetName());
				}

				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "Finished");
			}

			void Hierarchy::DrawHierarchyRecursive(const Entity& entity, const std::string& prefix, const bool isLast) {
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self }, "{}{}{}", prefix, isLast ? "└─" : "├─", entity.GetName());

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

			void Hierarchy::OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				e.DestroyChildren();
			}
		}
	}
}
