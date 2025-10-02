#pragma once
#include "System.hpp"
#include "WorldSystem/DisposableEntityPool.hpp"
#include "Graphics/Animator/QuadAnimator.hpp"
#include "WorldSystem/Tags.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class Animation final : public System {
			public:
				void OnTickUpdate(Core::GameTimer& gameTimer) override;

				std::optional<Entity> PlayAnimation(const glm::vec2 position, const int16_t depth, const glm::vec2 scale, const float rotation, const Graphics::IsAnimationWithParams auto&... sequence) {
					auto result = m_EntityPool.GetFreeEntity();
					if (result) {
						Entity entity = result.value().first;
						uint16_t index = result.value().second;
						auto& tr = entity.GetComponents<Components::Entity::Transform>();
						tr.SetLocalPosition(position);
						tr.SetLocalDepth(depth);
						tr.SetLocalScale(scale);
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
				DisposableEntityPool<32> m_EntityPool;
			};
		}
	}
}
