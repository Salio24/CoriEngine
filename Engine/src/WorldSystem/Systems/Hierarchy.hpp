#pragma once
#include "System.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class Hierarchy final : public System {
				public:

				bool Create();

				static constexpr SystemPriority Priority = 50;

			private:
				void OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity);
			};
		}
	}
}
