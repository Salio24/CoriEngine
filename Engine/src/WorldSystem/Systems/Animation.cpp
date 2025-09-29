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
				return true;
			}
		}
	}
}
