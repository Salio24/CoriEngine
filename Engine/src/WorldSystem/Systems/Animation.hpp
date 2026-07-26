#pragma once
//legacy from old 2d renderer, need to rewire
#if 0
#include "System.hpp"
#include "WorldSystem/DisposableEntityPool.hpp"
#include "Graphics/Animator/QuadAnimator.hpp"
#include "WorldSystem/Tags.hpp"

#ifndef CORI_DISPOSABLE_ANIMATION_POOL_SIZE
	#define CORI_DISPOSABLE_ANIMATION_POOL_SIZE 32
#endif

namespace Cori {
	namespace World {
		namespace Systems {
			/**
			 * @brief System that is responsible for Animations, every Scene has it by default.
			 */
			class Animation final : public System {
			public:
				void OnTickUpdate(Core::GameTimer& gameTimer) override;

				/**
				 * @brief Allows you to play animation sequence without having an entity to do so on. Fire and forget style.
				 * @details Internally it still uses an entity to play an animations on, but you don't have to worry about the entity creation and lifetime.
				 * It uses DisposableEntityPool so entities are reused.
				 * @param position Local position of the transform component of the entity the animation sequence will be played on.
				 * @param depth Local depth of the transform component of the entity the animation sequence will be played on.
				 * @param scale Local scale of the transform component of the entity the animation sequence will be played on.
				 * @param rotation Local rotation of the transform component of the entity the animation sequence will be played on.
				 * @param sequence Animation sequence to play.
				 * @return Optional object containing an entity that will be used to play an animation, or empty if all entities are currently busy.
				 * @note You can increase entity pool size by defining ```CORI_DISPOSABLE_ANIMATION_POOL_SIZE``` in the ```GlobalDefines.hpp``` file.
				 */
				std::optional<Entity> PlayAnimation(const glm::vec2 position, const int16_t depth, const glm::vec2 scale, const float rotation, const Graphics::IsAnimationWithParams auto&... sequence) {
					auto result = m_EntityPool.GetFreeEntity();
					if (result) {
						Entity entity = result.value().first;
						uint16_t index = result.value().second;
						auto& tr = entity.GetComponents<Components::Entity::Transform>();
						//tr.SetLocalPosition(position);
						tr.SetLocalDepth(depth);
						//tr.SetLocalScale(scale);
						tr.SetLocalRotation(rotation);

						auto& qa = entity.GetComponents<Components::Entity::QuadAnimator>();
						qa.Play(std::forward<decltype(sequence)>(sequence)...);

						qa.SetEngineStopCallback([this, index] {
							m_EntityPool.FreeIndex(index);
						});

						return entity;
					}

					CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Systems::Self, Logger::Tags::World::Systems::Animation }, "Failed to play independent animation, no free entities available.");
					return std::nullopt;
				}

				bool Create();

				static constexpr SystemPriority Priority = 200;
			private:
				void OnQuadAnimationCreate(entt::registry& registry, entt::entity entity);

				DisposableEntityPool<CORI_DISPOSABLE_ANIMATION_POOL_SIZE> m_EntityPool;
			};
		}
	}
}
#endif