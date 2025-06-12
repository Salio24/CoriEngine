#pragma once

namespace Cori {
	namespace Phyics {
		bool PointVsRect(const glm::vec2& point, const glm::vec2& boxSize, const glm::vec2& boxPos);

		bool RectVsRect(const glm::vec2 rect1Pos, const glm::vec2 rect1Size, const glm::vec2 rect2Pos, const glm::vec2 rect2Size);

		bool RayVsRect(const glm::vec2 rayOrigin, const glm::vec2 rayDirection, const glm::vec2 targetPos, const glm::vec2 targetSize, glm::vec2& contactPoint, glm::vec2& contactNormal, float& hitTimeNear);

		bool ResolvePenetrationOnX(glm::vec2& dynamicPos, glm::vec2& dynamicVel, glm::vec2 dynamicSize, glm::vec2 staticPos, glm::vec2 staticSize, glm::vec2& normal);

		bool ResolvePenetrationOnY(glm::vec2& dynamicPos, glm::vec2& dynamicVel, glm::vec2 dynamicSize, glm::vec2 staticPos, glm::vec2 staticSize, glm::vec2& normal);
	}
}