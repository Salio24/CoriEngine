#pragma once
#include <box2cpp/box2cpp.h>

#ifndef CORI_PIXELS_PER_METER
	#define CORI_PIXELS_PER_METER 16
#endif

namespace Cori {
	namespace Physics {

		bool PointVsRect(const glm::vec2& point, const glm::vec2& boxSize, const glm::vec2& boxPos);

		bool RectVsRect(const glm::vec2 rect1Pos, const glm::vec2 rect1Size, const glm::vec2 rect2Pos, const glm::vec2 rect2Size);

		bool RayVsRect(const glm::vec2 rayOrigin, const glm::vec2 rayDirection, const glm::vec2 targetPos, const glm::vec2 targetSize, glm::vec2& contactPoint, glm::vec2& contactNormal, float& hitTimeNear);

		using Vec2 = b2Vec2;
		using Rot = b2Rot;

		inline glm::vec2 ToPixels(const Vec2 vec) {
			return { vec.x * CORI_PIXELS_PER_METER, vec.y * CORI_PIXELS_PER_METER };
		}

		inline Vec2 ToMeters(const glm::vec2 vec) {
			return { vec.x / (float)CORI_PIXELS_PER_METER, vec.y / (float)CORI_PIXELS_PER_METER };
		}

		class ConvexHull {
		public:
			ConvexHull(const b2Hull& hull) : m_Hull(hull) {}

			ConvexHull Create(const std::vector<Vec2>& vertices) {
#ifdef DEBUG_BUILD
				m_Hull = b2ComputeHull(vertices.data(), vertices.size());
				if (m_Hull.count == 0) {
					std::string str_vertices;
					for (Vec2 val : vertices) {
						str_vertices += "(" + std::to_string(val.x) + ", " + std::to_string(val.x) + ")";
					}
					CORI_CORE_ASSERT_ERROR(m_Hull.count != 0, "Failed to create ConvexHull. Vertices: {}", str_vertices);
				}
				return m_Hull;
#endif
#ifndef DEBUG_BUILD
				return m_Hull = b2ComputeHull(vertices.data(), vertices.size());
#endif
			}

			operator b2Hull& () {
				return m_Hull;
			}

			operator const b2Hull* () const {
				return &m_Hull;
			}

		private:
			b2Hull m_Hull;
		};

		class Polygon {
		public:
			Polygon(const b2Polygon& polygon) : m_Polygon(polygon) {}

			static Polygon CreateBox(Vec2 size, float radius = 0.0f) {
				size.x /= 2.0f;
				size.y /= 2.0f;
				return b2MakeOffsetRoundedBox(size.x, size.y, size, Rot{ 1, 0 }, radius);
			}

			static Polygon CreateBox(Vec2 size, Vec2 offset, Rot rotation = Rot{ 1, 0 }, float radius = 0.0f) {
				return b2MakeOffsetRoundedBox(size.x / 2.0f, size.y / 2.0f, offset, rotation, radius);
			}

			static Polygon CreateBoxCentered(Vec2 size) {
				return b2MakeBox(size.x / 2.0f, size.y / 2.0f);
			}

			static Polygon CreateBoxCentered(Vec2 size, float radius) {
				return b2MakeRoundedBox(size.x / 2.0f, size.y / 2.0f, radius);
			}

			static Polygon CreatePolygon(const ConvexHull& hull, float radius = 0.0f) {
				return b2MakePolygon(hull, radius);
			}

			static Polygon CreatePolygon(const ConvexHull& hull, Vec2 offset, Rot rotation = Rot{ 1, 0 }, float radius = 0.0f) {
				return b2MakeOffsetRoundedPolygon(hull, offset, rotation, radius);
			}

			operator b2Polygon& () {
				return m_Polygon;
			}

			operator const b2Polygon* () const {
				return &m_Polygon;
			}

		private:
			b2Polygon m_Polygon;
		};

		class Circle {
		public:
			Circle(const b2Circle& circle) : m_Circle(circle) {}

			static Circle Create(Vec2 center, float radius) {
				return b2Circle{ {center}, radius };
			}

			operator b2Circle& () {
				return m_Circle;
			}

			operator const b2Circle* () const {
				return &m_Circle;
			}

		private:
			b2Circle m_Circle;
		};

		class Capsule {
		public:
			Capsule(const b2Capsule& capsule) : m_Capsule(capsule) {}

			static Capsule Create(Vec2 center1, Vec2 center2, float radius) {
				return b2Capsule{ {center1}, {center2}, radius };
			}

			operator b2Capsule& () {
				return m_Capsule;
			}

			operator const b2Capsule* () const {
				return &m_Capsule;
			}

		private:
			b2Capsule m_Capsule;
		};

		class PhysicsWorld : public World {
		public:
			PhysicsWorld() : World{ World::Params{} } {
			}
		};
	}
}