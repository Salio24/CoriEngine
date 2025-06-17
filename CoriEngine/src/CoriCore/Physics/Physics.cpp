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
	}
}