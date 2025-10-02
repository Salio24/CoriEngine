#include "Animation.hpp"
#include "Graphics/Animator/QuadAnimator.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			void Animation::OnTickUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();

				EntityView view = m_Owner.View<Components::Entity::QuadAnimator>(Exclude<Components::Entity::InactiveLocallyFlag>());

				for (const auto entity : view) {
					view.Get<Components::Entity::QuadAnimator>(entity).OnTickUpdate();
				}
			}

			bool Animation::Create() {
				m_EntityPool.Init(m_Owner,
				[](SceneHandle& scene) -> Entity {
					static uint32_t count = 0;
					Entity entity = scene.CreateEntity(std::format("Temporary entity {}", count), EntityTags::DisposableEntity);
					entity.AddComponent<Components::Entity::QuadRenderer>();
					entity.AddComponent<Components::Entity::QuadAnimator>(entity);
					++count;
					return entity;
				},
				[](Entity& entity) {
					auto& tr = entity.GetComponents<Components::Entity::Transform>();
					tr.SetLocalPosition({ 0.0f, 0.0f });
					tr.SetLocalDepth(0);
					tr.SetLocalScale({ 1.0f, 1.0f });
					tr.SetLocalRotation(0.0f);

					entity.UnlinkFromParent();
					entity.DestroyChildren();
				});


				return true;
			}
		}
	}
}
