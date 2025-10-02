#include "Hierarchy.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			bool Hierarchy::Create() {
				m_Owner.GetRegistry().on_destroy<Components::Entity::Hierarchy>().connect<&Hierarchy::OnHierarchyComponentDestroyed>(this);
				return true;
			}

			void Hierarchy::OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				e.DestroyChildren();
			}
		}
	}
}
