#pragma once
#include <entt/entt.hpp>
#include <box2d/box2d.h>
#include <box2cpp/box2cpp.h>

namespace Cori {
	class Scene;

	namespace Physics {
		enum class BodyType : uint8_t { Static = 0, Kinematic, Dynamic };
		class PhysicsWorld;

		using Vec2 = b2Vec2;
		using Rot = b2Rot;

		enum class PolygonType : uint8_t { Box = 0, Polygon };

		enum class ShapeType : uint8_t { Circle = 0, Capsule, Segment, Polygon, ChainSegment };

		class Polygon {
		public:
			Polygon(const b2Polygon& polygon) : m_Polygon(polygon) {}

			static Polygon CreateBox(Vec2 size) {
				return b2MakeOffsetBox(size.x, size.y, size, Rot{1, 0});
			}

			static Polygon CreateBox(Vec2 size, Vec2 offset, Rot rotation = Rot{1, 0}) {
				return b2MakeOffsetBox(size.x, size.y, offset, rotation);
			}

			static Polygon CreateBoxCentered(Vec2 size) {
				return b2MakeBox(size.x, size.y);
			}

			// at the rest of polygon types, setters getters etc

			operator b2Polygon&() {
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

			Circle(Vec2 center, float radius) {

			}

			static Circle Create(Vec2 center, float radius);

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

			Capsule(Vec2 center1, Vec2 center2 ,float radius) {

			}

			static Capsule Create(Vec2 center1, Vec2 center2, float radius);

			operator b2Capsule& () {
				return m_Capsule;
			}

			operator const b2Capsule* () const {
				return &m_Capsule;
			}

		private:
			b2Capsule m_Capsule;
		};

		// segments ughhh....

		struct Rigidbody_EntityComponent : public BodyRef {
			Rigidbody_EntityComponent(WorldRef world, const std::derived_from<b2BodyDef> auto& def) : BodyRef{ world.CreateBody(DestroyWithParent, def) } {}
			~Rigidbody_EntityComponent() { if (IsValid()) { Destroy(); } }
		};

		class PhysicsWorld : public World {
		public:
			PhysicsWorld() : World{ World::Params{} } {
			}
		};
	}

}