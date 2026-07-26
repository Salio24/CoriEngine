//legacy from old 2d renderer, need to rewire
#if 0
#include "Animation.hpp"
#include "Graphics/Animator/QuadAnimator.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			void Animation::OnTickUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();

				StaticEntityView view = m_Owner.StaticView<Components::Entity::QuadAnimator>(Exclude<Components::Entity::InactiveLocallyFlag>());

				for (const auto entity : view) {
					view.Get<Components::Entity::QuadAnimator>(entity).OnTickUpdate();
				}
			}

			bool Animation::Create() {
				m_Owner.GetRegistry().on_construct<Components::Entity::QuadAnimator>().connect<&Animation::OnQuadAnimationCreate>(this);

				m_EntityPool.Init(m_Owner,
				[](SceneHandle& scene) -> Entity {
					static uint32_t count = 0;
					Entity entity = scene.CreateEntity<EntityTags::DisposableEntityTag>(std::format("Temporary entity {}", count));
					entity.AddComponent<Components::Entity::QuadRenderer>();
					entity.AddComponent<Components::Entity::QuadAnimator>();
					++count;
					return entity;
				},
				[](Entity& entity) {
					auto& tr = entity.GetComponents<Components::Entity::Transform>();
					tr.SetLocalScale({ 0.0f, 0.0f, 0.0f });
					entity.UnlinkFromParent();
					entity.DestroyChildren();
				});

				return true;
			}

			void Animation::OnQuadAnimationCreate(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& qa = e.GetComponents<Components::Entity::QuadAnimator>();
				qa.m_Entity = e;
				auto& renderer = e.GetOrAddComponent<Components::Entity::QuadRenderer>();
				renderer.m_AnimatorBound = true;
			}
		}
	}
}
#endif