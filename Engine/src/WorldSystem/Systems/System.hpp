#pragma once
#include "WorldSystem/SceneHandle.hpp"
#include "Concept.hpp"

//* \n SystemPriority can be used to ensure one system is updated before another, it is safe to have more than one system with the same priority,
//* but I highly discourage toy from doing that as in that case the order is not enforced and can vary from run to run.

namespace Cori {
	namespace World {
		class System {
		public:
			System() = default;
			virtual ~System() = default;

			virtual void OnUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {}

			virtual void OnTickUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {}

			virtual void OnImGuiRender([[maybe_unused]] Core::GameTimer& gameTimer) {}

		protected:
			SceneHandle m_Owner{ nullptr };
		private:
			friend Scene;
			void SetOwnerScene(const Scene* scene);
		};
	}
}
