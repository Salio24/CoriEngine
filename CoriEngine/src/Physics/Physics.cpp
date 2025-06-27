#include "Physics.hpp"

namespace Cori {
	namespace Physics {
		bool PointVsRect(const glm::vec2 point, const glm::vec2 boxSize, const glm::vec2 boxPos) {
			return (point.x >= boxPos.x && point.y >= boxPos.y && point.x < boxPos.x + boxSize.x && point.y < boxPos.y + boxSize.y);
		}

		bool RectVsRect(const glm::vec2 rect1Pos, const glm::vec2 rect1Size, const glm::vec2 rect2Pos, const glm::vec2 rect2Size) {
			return (rect1Pos.x < rect2Pos.x + rect2Size.x && rect1Pos.x + rect1Size.x > rect2Pos.x && rect1Pos.y < rect2Pos.y + rect2Size.y && rect1Pos.y + rect1Size.y > rect2Pos.y);
		}

		bool RayVsRect(const glm::vec2 rayOrigin, const glm::vec2 rayDirection, const glm::vec2 targetPos, const glm::vec2 targetSize, glm::vec2& contactPoint, glm::vec2& contactNormal, float& hitTimeNear) {
			contactNormal = { 0,0 };
			contactPoint = { 0,0 };

			// Cache division
			glm::vec2 invDirection = 1.0f / rayDirection;

			// Calculate intersections with rectangle bounding axes
			glm::vec2 timeNear = (targetPos - rayOrigin) * invDirection;
			glm::vec2 timeFar = (targetPos + targetSize - rayOrigin) * invDirection;

			if (std::isnan(timeFar.y) || std::isnan(timeFar.x)) {
				return false;
			}
			if (std::isnan(timeNear.y) || std::isnan(timeNear.x)) {
				return false;
			}

			// Sort distances
			if (timeNear.x > timeFar.x) {
				std::swap(timeNear.x, timeFar.x);
			}
			if (timeNear.y > timeFar.y) {
				std::swap(timeNear.y, timeFar.y);
			}

			// Early rejection
			if (timeNear.x > timeFar.y || timeNear.y > timeFar.x) {
				return false;
			}

			// Closest 'time' will be the first contact
			hitTimeNear = std::max(timeNear.x, timeNear.y);

			// Furthest 'time' is contact on opPositionite side of target
			float hitTimeFar = std::min(timeFar.x, timeFar.y);

			// Reject if ray direction is pointing away from object
			if (hitTimeFar < 0) {
				return false;
			}

			// Contact point of collision from parametric line equation
			contactPoint = rayOrigin + hitTimeNear * rayDirection;

			if (timeNear.x > timeNear.y) {
				if (invDirection.x < 0) {
					contactNormal = { 1, 0 };
				}
				else {
					contactNormal = { -1, 0 };
				}
			}
			else if (timeNear.x < timeNear.y) {
				if (invDirection.y < 0) {
					contactNormal = { 0, 1 };
				}
				else {
					contactNormal = { 0, -1 };
				}
			}
			return true;
		}


		WindingOrder GetPolygonWindingOrder(const std::vector<Vec2>& polygon) {
			int n = polygon.size();
			if (n < 3) {
				return WindingOrder::COLLINEAR;
			}

			double signedAreaSum = 0.0f;

			for (int i = 0; i < n; ++i) {
				const Vec2& p1 = polygon.at(i);
				const Vec2& p2 = polygon.at((i + 1) % n);
				signedAreaSum += (p1.x * p2.y) - (p2.x * p1.y);
			}

			double epsilon = 1e-9;

			if (signedAreaSum > epsilon) {
				return WindingOrder::CLOCKWISE;
			}
			else if (signedAreaSum < -epsilon) {
				return WindingOrder::COUNTER_CLOCKWISE;
			}
			else {
				return WindingOrder::COLLINEAR;
			}
		}

		WindingOrder GetPolygonWindingOrder(const std::vector<tmx::Vector2f>& polygon) {
			int n = polygon.size();
			if (n < 3) {
				return WindingOrder::COLLINEAR;
			}

			double signedAreaSum = 0.0f;

			for (int i = 0; i < n; ++i) {
				const tmx::Vector2f& p1 = polygon.at(i);
				const tmx::Vector2f& p2 = polygon.at((i + 1) % n);

				signedAreaSum += (p1.x * p2.y) - (p2.x * p1.y);
			}

			double epsilon = 1e-9;

			if (signedAreaSum > epsilon) {
				return WindingOrder::CLOCKWISE;
			}
			else if (signedAreaSum < -epsilon) {
				return WindingOrder::COUNTER_CLOCKWISE;
			}
			else {
				return WindingOrder::COLLINEAR;
			}
		}

		std::string WindingOrderToString(WindingOrder order) {
			switch (order) {
			case WindingOrder::COLLINEAR:
				return "Collinear";
				break;
			case WindingOrder::CLOCKWISE:
				return "Clockwise (CW)";
				break;
			case WindingOrder::COUNTER_CLOCKWISE:
				return "Counter-Clockwise (CCW)";
				break;
			}
		}

		glm::vec2 ToPixels(const Vec2 vec) {
			return { vec.x * CORI_PIXELS_PER_METER, vec.y * CORI_PIXELS_PER_METER };
		}

		Vec2 ToMeters(const glm::vec2 vec) {
			return { vec.x / (float)CORI_PIXELS_PER_METER, vec.y / (float)CORI_PIXELS_PER_METER };
		}

		std::string Vec2ToString(Vec2 vec) {
			return std::string("(") + std::to_string(vec.x) + std::string(", ") + std::to_string(vec.y) + std::string(")");
		}
	}
}