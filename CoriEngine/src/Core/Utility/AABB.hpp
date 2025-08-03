#pragma once

namespace Cori {
	namespace Utils {
		struct AABB {
			glm::vec2 m_Min;
			glm::vec2 m_Max;
		};

		[[nodiscard]] inline AABB CalculateAABB(const glm::mat3& transform, const glm::vec2 halfSize) {
			glm::vec2 corners[4]{
					{transform * glm::vec3{-halfSize.x, -halfSize.y, 1.0f}},
					{transform * glm::vec3{halfSize.x, -halfSize.y, 1.0f}},
					{transform * glm::vec3{halfSize.x, halfSize.y, 1.0f}},
					{transform * glm::vec3{-halfSize.x, halfSize.y, 1.0f}},
				};
			AABB bounds;

			bounds.m_Max = corners[0];
			bounds.m_Min = corners[0];

			for (int i = 1; i < 4; ++i) {
				bounds.m_Min.x = std::min(bounds.m_Min.x, corners[i].x);
				bounds.m_Min.y = std::min(bounds.m_Min.y, corners[i].y);
				bounds.m_Max.x = std::max(bounds.m_Max.x, corners[i].x);
				bounds.m_Max.y = std::max(bounds.m_Max.y, corners[i].y);
			}

			return bounds;
		}

		[[nodiscard]] inline bool AABBOverlapCheck(const AABB& a, const AABB& b) {
			return (a.m_Min.x <= b.m_Max.x && a.m_Max.x >= b.m_Min.x) &&
				   (a.m_Min.y <= b.m_Max.y && a.m_Max.y >= b.m_Min.y);
		}
	}
}
