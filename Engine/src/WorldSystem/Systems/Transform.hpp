#pragma once
#include "System.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class Transform final : public System {
			public:

				void OnUpdate(Core::GameTimer& gameTimer) override;

				bool Create();

				static constexpr SystemPriority Priority = 50;

			private:
				void UpdateTransform();
				void UpdateTransformRecursive(entt::entity entity, const glm::mat3& parentTransform, const uint8_t parentDepth, const bool parentTransformDirty, const bool parentDepthDirty);

				void OnTransformCreate(entt::registry& registry, entt::entity entity);
			};
		}
	}
}
