#include "Math.hpp"

namespace {
	glm::vec2 RK4_AccelerationVec2(const Cori::Math::PhysicsState& state, double time, glm::vec2 acceleratinon) {
		return acceleratinon;
	}

	Cori::Math::Derivative RK4_EvaluateVec2(Cori::Math::PhysicsState& initial, glm::vec2 acceleratinon, double time, double timeStep, const Cori::Math::Derivative& d) {
		Cori::Math::PhysicsState state;

		state.position = initial.position + d.dpos, (float)timeStep;
		state.velocity = initial.velocity + d.dvel, (float)timeStep;

		Cori::Math::Derivative output;
		output.dpos = state.velocity;
		output.dvel = RK4_AccelerationVec2(state, time + timeStep, acceleratinon);

		return output;
	}
}

namespace Cori {
	namespace Math {
		void RK4_IntegrateVec2(PhysicsState& state, double time, double timeStep, glm::vec2 acceleration) {
			Derivative a, b, c, d;

			a = RK4_EvaluateVec2(state, acceleration, time, 0.0f, Derivative());
			b = RK4_EvaluateVec2(state, acceleration, time, timeStep * 0.5f, a);
			c = RK4_EvaluateVec2(state, acceleration, time, timeStep * 0.5f, b);
			d = RK4_EvaluateVec2(state, acceleration, time, timeStep, c);

			glm::vec2 positionChange = 1.0f / 6.0f * (a.dpos + 2.0f * (b.dpos + c.dpos) + d.dpos);
			glm::vec2 velocityChange = 1.0f / 6.0f * (a.dvel + 2.0f * (b.dvel + c.dvel) + d.dvel);

			state.position = state.position + positionChange * (float)timeStep;
			state.velocity = state.velocity + velocityChange * (float)timeStep;
		}
	}
}