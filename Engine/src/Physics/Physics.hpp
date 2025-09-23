#pragma once
#include <box2cpp/box2cpp.h>
#include <tmxlite/Map.hpp>
#include "WorldSystem/Entity.hpp"

#ifndef CORI_PIXELS_PER_METER
	#define CORI_PIXELS_PER_METER 16
#endif

namespace Cori {
	/**
	 * @brief Anything connected to physics is in this namespace. Please refer to Box2D docs 'https://box2d.org/' for any details regarding physics.
	 * @details Cori engine doesn't have a native physics engine and uses Box2D, so refer to Box2D docs 'https://box2d.org/' for any details on physics.
	 * \n All the engine does is provide a convenient C++ API for it, as Box2D is a C project and the default API is not really convenient in C++ environment.
	 * \n Big thanks HolyBlackCat for: 'https://github.com/HolyBlackCat/box2cpp/tree/master'
	 */
	namespace Physics {

		/**
		 * @brief An alias for Box2D native vec2 type, if you see it somewhere, be sure data there is in meters contrary to when glm used, there data is in camera pixels.
		 */
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

			explicit ShapeUserData(const Cori::World::Entity& entity) : m_Entity(entity) {}

			Cori::World::Entity m_Entity{};
		};

		struct CastResult {
			CastResult() = default;

			bool hit{ false };
			float fraction;
			Vec2 point;
			Vec2 normal;
			ShapeRef shape;
		};

		/**
		 * @brief Calculates the winding order of the polygon made up from individual points. Box2D version. Takes Box2Ds vec2s.
		 * @param polygon Point that from a polygon.
		 * @return A resulting enumerator of type WindingOrder.
		 */
		WindingOrder GetPolygonWindingOrder(const std::vector<Vec2>& polygon);

		/**
		 * @brief Calculates the winding order of the polygon made up from individual points. TMXLite version. Takes TMXLite vec2s.
		 * @param polygon Point that from a polygon.
		 * @return A resulting enumerator of type WindingOrder.
		 */
		WindingOrder GetPolygonWindingOrder(const std::vector<tmx::Vector2f>& polygon);

		/**
		 * @brief Converts the WindingOrder enumerator to string, for logging.
		 * @param order Enumerator.
		 * @return Resulting string.
		 */
		const char* WindingOrderToString(WindingOrder order);


		/**
		 * @brief Converts physical meters to camera space pixels.
		 * @param vec Position in meters.
		 * @return Position in pixels.
		 * @note Talking about camera space pixels, not screen space/viewport pixels. Uses CORI_PIXELS_PER_METER as a convertion modifier.
		 */
		glm::vec2 ToPixels(const Vec2 vec);

		/**
		 * @brief Converts camera space pixels to physical meters.
		 * @param vec Position in pixels.
		 * @return Position in pixels.
		 * @note Talking about camera space pixels, not screen space/viewport pixels. Uses CORI_PIXELS_PER_METER as a convertion modifier.
		 */
		Vec2 ToMeters(const glm::vec2 vec);

		/**
		 * @brief Converts a native Box2D vec2 to string, for logging.
		 * @param vec Native Box2D vec2 to convert to string.
		 * @return Formated string.
		 * @note This is a convenience function and will cause several allocation connected to constructing a string. Be aware.
		 */
		std::string Vec2ToString(Vec2 vec);

		class ConvexHull {
		public:
			ConvexHull() = default;

			ConvexHull(const b2Hull& hull) : m_Hull(hull) {} // NOLINT

			[[nodiscard]] static ConvexHull Create(const std::vector<Vec2>& vertices) {
#ifdef DEBUG_BUILD
				const b2Hull hull = b2ComputeHull(vertices.data(), vertices.size());
				if (hull.count == 0) {
					std::string str_vertices;
					for (auto [x, y] : vertices) {
						str_vertices += "(" + std::to_string(x) + ", " + std::to_string(x) + ")";
					}
					CORI_CORE_CHECK(hull.count != 0, "Failed to create ConvexHull. Vertices: {}", str_vertices);
				}
				return hull;
#endif
#ifndef DEBUG_BUILD
				return b2ComputeHull(vertices.data(), vertices.size());
#endif
			}

			operator b2Hull& () { // NOLINT
				return m_Hull;
			}

			operator const b2Hull* () const { // NOLINT
				return &m_Hull;
			}

		private:
			b2Hull m_Hull;
		};

		class Polygon : public b2Polygon {
		public:
			Polygon() = default;

			Polygon(const b2Polygon& polygon) : b2Polygon(polygon) {} // NOLINT

			[[nodiscard]] static Polygon CreateBox(const Vec2 halfSize, const Vec2 offset, const Rot rotation = Rot{ 1, 0 }, const float radius = 0.0f) {
				return b2MakeOffsetRoundedBox(halfSize.x, halfSize.y, offset, rotation, radius);
			}

			[[nodiscard]] static Polygon CreateBox(const Vec2 halfSize) {
				return b2MakeBox(halfSize.x, halfSize.y);
			}

			[[nodiscard]] static Polygon CreateBox(const Vec2 halfSize, const float radius) {
				return b2MakeRoundedBox(halfSize.x, halfSize.y, radius);
			}

			[[nodiscard]] static Polygon CreatePolygon(const ConvexHull& hull, const float radius = 0.0f) {
				return b2MakePolygon(hull, radius);
			}

			[[nodiscard]] static Polygon CreatePolygon(const ConvexHull& hull, const Vec2 offset, const Rot rotation = Rot{ 1, 0 }, const float radius = 0.0f) {
				return b2MakeOffsetRoundedPolygon(hull, offset, rotation, radius);
			}
		};

		class Circle : public b2Circle {
		public:
			Circle() = default;

			Circle(const b2Circle& circle) : b2Circle(circle) {} // NOLINT
			Circle(const Vec2 center, const float radius) : b2Circle{center, radius} {}

			[[nodiscard]] static Circle Create(Vec2 center, float radius) {
				return { center, radius };
			}
		};

		class Capsule : public b2Capsule {
		public:
			Capsule() = default;

			Capsule(const b2Capsule& capsule) : b2Capsule(capsule) {} // NOLINT
			Capsule(const Vec2 center1, const Vec2 center2, const float radius) : b2Capsule{ center1, center2, radius } {}

			[[nodiscard]] static Capsule Create(Vec2 center1, Vec2 center2, float radius) {
				return { center1, center2, radius };
			}
		};

		class Segment : public b2Segment {
		public:
			Segment() = default;

			Segment(const b2Segment& segment) : b2Segment(segment) {} // NOLINT
			Segment(const Vec2 point1, const Vec2 point2) : b2Segment{ point1, point2 } {}

			[[nodiscard]] static Segment Create(Vec2 point1, Vec2 point2) {
				return { point1, point2 };
			}
		};

		class PhysicsWorld : public World {
		public:
			PhysicsWorld() : World{ Params{} } {
			}
		};
	}
}

inline void operator*=(Cori::Physics::Vec2& a, Cori::Physics::Vec2 b)
{
	a.x *= b.x;
	a.y *= b.y;
}