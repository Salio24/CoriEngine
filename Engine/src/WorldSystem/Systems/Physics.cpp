#include "Physics.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			bool Physics::Create() {
				m_Owner.GetRegistry().on_construct<Components::Entity::RigidBody>().connect<&Physics::OnRigidBodyCreate>(this);
				return true;
			}

			void Physics::OnRigidBodyCreate(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& rb = e.GetComponents<Components::Entity::RigidBody>();
				const auto type = rb.GetType();
				if (type == b2_kinematicBody || type == b2_dynamicBody) {
					auto& ud = e.AddComponent<Cori::Physics::BodyUserData>();
					rb.SetUserData(&ud);
				}
			}
		}
	}
}