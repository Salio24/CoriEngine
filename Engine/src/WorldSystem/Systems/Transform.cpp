#include "Transform.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			void Transform::OnUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();
				UpdateTransform();
			}

			void Transform::UpdateTransform() {
				const auto view1 = m_Owner.GetRegistry().view<Components::Entity::Internal::DirtyTransformFlag>();

				for (const auto entity : view1) {
					UpdateTransformRecursive(entity, glm::mat3(1.0f), 1, false, false);
				}
				m_Owner.GetRegistry().clear<Components::Entity::Internal::DirtyTransformFlag>();
			}

			void Transform::UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, const uint8_t parentDepth, const bool parentTransformDirty, const bool parentDepthDirty) {
				auto& transform = m_Owner.GetRegistry().get<Components::Entity::Transform>(entity);
				const bool transformDirty = transform.m_DirtyTransform || parentTransformDirty;
				const bool layerDirty = transform.m_DirtyDepth || parentDepthDirty;

				if (transformDirty) {
					if (!transform.GetDetachedState()) {
						transform.m_WorldTransform = parentTransform * transform.GetLocalTransform();
						transform.m_LastParentTransform = parentTransform;
					} else {
						transform.m_WorldTransform = transform.m_LastParentTransform * transform.GetLocalTransform();
					}
					transform.m_DirtyTransform = false;
				}
				if (layerDirty) {
					int16_t unclamped = parentDepth + transform.GetLocalDepthOffset();
					if (unclamped < 0 || unclamped > 255) {
						uint8_t clamped = static_cast<uint8_t>(std::clamp(unclamped, static_cast<int16_t>(0), static_cast<int16_t>(255)));
						CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Final calculated depth for Entity '{}' is '{}' which is outside of allowed range [0, 255], it will be clamped to '{}'", Entity{ { m_Owner.GetRegistry(), entity } }.GetDebugData(), unclamped, clamped);
						transform.m_WorldDepth = clamped;
						transform.m_DirtyDepth = false;
					} else {
						transform.m_WorldDepth = static_cast<uint8_t>(unclamped);
						transform.m_DirtyDepth = false;
					}
				}
				const auto& hierarchy = m_Owner.GetRegistry().get<Components::Entity::Hierarchy>(entity);
				entt::entity currentChild = hierarchy.m_FirstChild;
				while (m_Owner.GetRegistry().valid(currentChild)) {
					UpdateTransformRecursive(currentChild, transform.m_WorldTransform, transform.m_WorldDepth, transformDirty, layerDirty);
					currentChild = m_Owner.GetRegistry().get<Components::Entity::Hierarchy>(currentChild).m_NextSibling;
				}

			}

			bool Transform::Create() {
				m_Owner.GetRegistry().on_construct<Components::Entity::Transform>().connect<&Transform::OnTransformCreate>(this);
				return true;
			}

			void Transform::OnTransformCreate(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& tr = e.GetComponents<Components::Entity::Transform>();
				tr.m_Owner = e;
			}
		}
	}
}
