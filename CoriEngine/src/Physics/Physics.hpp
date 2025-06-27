#pragma once
#include <box2cpp/box2cpp.h>
#include <tmxlite/Map.hpp>
#include "SceneSystem/Entity.hpp"

#ifndef CORI_PIXELS_PER_METER
	#define CORI_PIXELS_PER_METER 16
#endif

namespace Cori {
	namespace Physics {

		using Vec2 = b2Vec2;
		using Rot = b2Rot;
		using Transform = b2Transform;
		using CollisionPlane = b2CollisionPlane;
		using RayResult = b2RayResult;


		enum CollisionBits : uint64_t {
			StaticBit   = 1ull << 0,
			MoverBit    = 1ull << 1,
			DynamicBit  = 1ull << 2,
			DebrisBit   = 1ull << 3,
			SensorBit   = 1ull << 4,
			CustomBit5  = 1ull << 5,
			CustomBit6  = 1ull << 6,
			CustomBit7  = 1ull << 7,
			CustomBit8  = 1ull << 8,
			CustomBit9  = 1ull << 9,
			CustomBit10 = 1ull << 10,
			CustomBit11 = 1ull << 11,
			CustomBit12 = 1ull << 12,
			CustomBit13 = 1ull << 13,
			CustomBit14 = 1ull << 14,
			CustomBit15 = 1ull << 15,
			CustomBit16 = 1ull << 16,
			CustomBit17 = 1ull << 17,
			CustomBit18 = 1ull << 18,
			CustomBit19 = 1ull << 19,
			CustomBit20 = 1ull << 20,
			CustomBit21 = 1ull << 21,
			CustomBit22 = 1ull << 22,
			CustomBit23 = 1ull << 23,
			CustomBit24 = 1ull << 24,
			CustomBit25 = 1ull << 25,
			CustomBit26 = 1ull << 26,
			CustomBit27 = 1ull << 27,
			CustomBit28 = 1ull << 28,
			CustomBit29 = 1ull << 29,
			CustomBit30 = 1ull << 30,
			CustomBit31 = 1ull << 31,
			CustomBit32 = 1ull << 32,
			CustomBit33 = 1ull << 33,
			CustomBit34 = 1ull << 34,
			CustomBit35 = 1ull << 35,
			CustomBit36 = 1ull << 36,
			CustomBit37 = 1ull << 37,
			CustomBit38 = 1ull << 38,
			CustomBit39 = 1ull << 39,
			CustomBit40 = 1ull << 40,
			CustomBit41 = 1ull << 41,
			CustomBit42 = 1ull << 42,
			CustomBit43 = 1ull << 43,
			CustomBit44 = 1ull << 44,
			CustomBit45 = 1ull << 45,
			CustomBit46 = 1ull << 46,
			CustomBit47 = 1ull << 47,
			CustomBit48 = 1ull << 48,
			CustomBit49 = 1ull << 49,
			CustomBit50 = 1ull << 50,
			CustomBit51 = 1ull << 51,
			CustomBit52 = 1ull << 52,
			CustomBit53 = 1ull << 53,
			CustomBit54 = 1ull << 54,
			CustomBit55 = 1ull << 55,
			CustomBit56 = 1ull << 56,
			CustomBit57 = 1ull << 57,
			CustomBit58 = 1ull << 58,
			CustomBit59 = 1ull << 59,
			CustomBit60 = 1ull << 60,
			CustomBit61 = 1ull << 61,
			CustomBit62 = 1ull << 62,
			CustomBit63 = 1ull << 63,

			AllBits = ~0u,
		};
		
		enum class WindingOrder : uint8_t {
			COLLINEAR,
			CLOCKWISE,
			COUNTER_CLOCKWISE
		};

		struct ShapeUserData {
			ShapeUserData() = default;

			ShapeUserData(const Entity& entity) : m_Entity(entity) {}

			Entity m_Entity{};
		};

		struct CastResult {
			CastResult() = default;

			bool hit{ false };
			float fraction;
			Vec2 point;
			Vec2 normal;
			ShapeRef shape;
		};

		// maybe remove them? do i even need those? idk
		bool PointVsRect(const glm::vec2& point, const glm::vec2& boxSize, const glm::vec2& boxPos);
		bool RectVsRect(const glm::vec2 rect1Pos, const glm::vec2 rect1Size, const glm::vec2 rect2Pos, const glm::vec2 rect2Size);
		bool RayVsRect(const glm::vec2 rayOrigin, const glm::vec2 rayDirection, const glm::vec2 targetPos, const glm::vec2 targetSize, glm::vec2& contactPoint, glm::vec2& contactNormal, float& hitTimeNear);

		WindingOrder GetPolygonWindingOrder(const std::vector<Vec2>& polygon);
		WindingOrder GetPolygonWindingOrder(const std::vector<tmx::Vector2f>& polygon);
		std::string WindingOrderToString(WindingOrder order);

		glm::vec2 ToPixels(const Vec2 vec);
		Vec2 ToMeters(const glm::vec2 vec);

		std::string Vec2ToString(Vec2 vec);

		class ConvexHull {
		public:
			ConvexHull() = default;

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

		class Polygon : public b2Polygon {
		public:
			Polygon() = default;

			Polygon(const b2Polygon& polygon) : b2Polygon(polygon) {}

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
		};

		class Circle : public b2Circle {
		public:
			Circle() = default;

			Circle(const b2Circle& circle) : b2Circle(circle) {}
			Circle(Vec2 center, float radius) : b2Circle{center, radius} {}

			static Circle Create(Vec2 center, float radius) {
				return { center, radius };
			}
		};

		class Capsule : public b2Capsule {
		public:
			Capsule() = default;

			Capsule(const b2Capsule& capsule) : b2Capsule(capsule) {}
			Capsule(Vec2 center1, Vec2 center2, float radius) : b2Capsule{ center1, center2, radius } {}

			static Capsule Create(Vec2 center1, Vec2 center2, float radius) {
				return { center1, center2, radius };
			}
		};

		class Segment : public b2Segment {
		public:
			Segment() = default;

			Segment(const b2Segment& segment) : b2Segment(segment) {}
			Segment(Vec2 point1, Vec2 point2) : b2Segment{ point1, point2 } {}

			static Segment Create(Vec2 point1, Vec2 point2) {
				return { point1, point2 };
			}
		};

		class PhysicsWorld : public World {
		public:
			PhysicsWorld() : World{ World::Params{} } {
			}
		};
	}
}