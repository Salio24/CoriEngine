#pragma once

namespace Cori {
	namespace Math {

		struct PhysicsState
		{
			glm::vec2 position{ 0.0f };
			glm::vec2 velocity{ 0.0f };
		};

		struct Derivative
		{
			glm::vec2 dpos{ 0.0f };
			glm::vec2 dvel{ 0.0f };
		};

		void RK4_IntegrateVec2(PhysicsState& state, double time, double timeStep, glm::vec2 acceleration);
	}
}