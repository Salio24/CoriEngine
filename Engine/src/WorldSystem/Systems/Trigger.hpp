#pragma once
#include "System.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class Trigger final : public System {
				public:

				void OnTickUpdate(Core::GameTimer& gameTimer) override;

				bool Create();

				static constexpr SystemPriority Priority = 300;
			};
		}
	}
}
