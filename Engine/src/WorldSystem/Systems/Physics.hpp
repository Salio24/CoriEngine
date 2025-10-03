#pragma once
#include "System.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class Physics final : public System {
			public:

				bool Create();

				static constexpr SystemPriority Priority = 5000;

			private:
				void OnRigidBodyCreate(entt::registry& registry, entt::entity entity);
			};
		}
	}
}