#include "Physics.hpp"

namespace Cori {
	namespace Physics {
		WindingOrder GetPolygonWindingOrder(const std::vector<Vec2>& polygon) {
			const int32_t n = polygon.size();
			if (n < 3) {
				return WindingOrder::COLLINEAR;
			}

			double signedAreaSum = 0.0f;

			for (int32_t i = 0; i < n; ++i) {
				const auto& [x1, y1] = polygon.at(i);
				const auto& [x2, y2] = polygon.at((i + 1) % n);
				signedAreaSum += x1 * y2 - x2 * y1;
			}

			constexpr double epsilon = 1e-9;

			if (signedAreaSum > epsilon) {
				return WindingOrder::CLOCKWISE;
			}
			if (signedAreaSum < -epsilon) {
				return WindingOrder::COUNTER_CLOCKWISE;
			}
			return WindingOrder::COLLINEAR;
		}

		WindingOrder GetPolygonWindingOrder(const std::vector<tmx::Vector2f>& polygon) {
			const int32_t n = polygon.size();
			if (n < 3) {
				return WindingOrder::COLLINEAR;
			}

			double signedAreaSum = 0.0f;

			for (int32_t i = 0; i < n; ++i) {
				const tmx::Vector2f& p1 = polygon.at(i);
				const tmx::Vector2f& p2 = polygon.at((i + 1) % n);

				signedAreaSum += p1.x * p2.y - p2.x * p1.y;
			}

			constexpr double epsilon = 1e-9;

			if (signedAreaSum > epsilon) {
				return WindingOrder::CLOCKWISE;
			}
			if (signedAreaSum < -epsilon) {
				return WindingOrder::COUNTER_CLOCKWISE;
			}
			return WindingOrder::COLLINEAR;
		}

		const char* WindingOrderToString(const WindingOrder order) {
			switch (order) {
			case WindingOrder::COLLINEAR:
				return "Collinear";
			case WindingOrder::CLOCKWISE:
				return "Clockwise (CW)";
			case WindingOrder::COUNTER_CLOCKWISE:
				return "Counter-Clockwise (CCW)";
			default:
				return "Unknown";
			}
		}

		// camera space "pixels" not screen space pixels
		glm::vec2 ToPixels(const Vec2 vec) {
			return { vec.x * CORI_PIXELS_PER_METER, vec.y * CORI_PIXELS_PER_METER };
		}

		Vec2 ToMeters(const glm::vec2 vec) {
			return { vec.x / static_cast<float>(CORI_PIXELS_PER_METER), vec.y / static_cast<float>(CORI_PIXELS_PER_METER) };
		}

		std::string Vec2ToString(const Vec2 vec) {
			return std::string("(") + std::to_string(vec.x) + std::string(", ") + std::to_string(vec.y) + std::string(")");
		}
	}
}