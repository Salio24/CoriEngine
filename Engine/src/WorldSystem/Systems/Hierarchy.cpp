#include "Hierarchy.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			bool Hierarchy::Create() {
				m_Owner.GetRegistry().on_destroy<Components::Entity::Hierarchy>().connect<&Hierarchy::OnHierarchyComponentDestroyed>(this);
				return true;
			}

			void Hierarchy::OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity) {

				const auto& hierarchy = registry.get<Components::Entity::Hierarchy>(entity);
				entt::entity currentChild = hierarchy.m_FirstChild;
				while (registry.valid(currentChild)) {
					const entt::entity nextChild = registry.get<Components::Entity::Hierarchy>(currentChild).m_NextSibling;
					registry.destroy(currentChild); // This triggers a recursive call for grandchildren.
					currentChild = nextChild;
				}
			}
		}
	}
}
