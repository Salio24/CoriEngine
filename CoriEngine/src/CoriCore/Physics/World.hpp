#pragma once
#include <entt/entt.hpp>
#include <box2d/box2d.h>

namespace Cori {
	class Scene;

	namespace Physics {
		class World;


		class Shape {
		public:
			Shape(b2ShapeId id) : m_ShapeID(id) {}

			operator b2ShapeId() {
				return m_ShapeID;
			}

		private:
			b2ShapeId m_ShapeID{ b2_nullShapeId };

		};

		enum class BodyType { Static = 0, Kinematic, Dynamic };
		enum class PolygonType { Box = 0, Polyhon };

		class Polygon {
		public:
			Polygon(glm::vec2 size) {

			}

			static Polygon CreateBox(glm::vec2 size);

			// at the rest of polygon types, setters getters etc

			operator const b2Polygon&() const {
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
			Circle(glm::vec2 center, float radius) {

			}

			static Circle Create(glm::vec2 center, float radius);

			operator const b2Circle& () const {
				return m_Circle;
			}

			operator const b2Circle* () const {
				return &m_Circle;
			}

		private:
			b2Circle m_Circle;
		};

		// capsules
		// segments ughhh....



	
		struct Rigidbody_EntityComponent {
			Rigidbody_EntityComponent(BodyType type, glm::vec2 position) : m_Type(type), m_Position(position) {}

			Shape AddShape(const Polygon& polygon, bool enableSensorEvents = false, bool isSensor = false, bool enableContactEvents = false /*filter*/) {
				b2ShapeDef def = b2DefaultShapeDef();
				def.enableSensorEvents = enableSensorEvents;
				def.isSensor = isSensor;
				def.enableContactEvents = enableContactEvents;

				return Shape(b2CreatePolygonShape(m_BodyID, &def, polygon));

			}

			// setters getters

			operator b2BodyId() {
				return m_BodyID;
			}

			glm::vec2 m_Position;
			BodyType m_Type;
		private:
			friend class Scene;
			friend class World;


			b2BodyId m_BodyID{ b2_nullBodyId };

		};
	
		
	

		class World {
		public:
			World() {
				b2WorldDef def = b2DefaultWorldDef();
				m_WorldID = b2CreateWorld(&def);
			}
			~World() {
				b2DestroyWorld(m_WorldID);
			}

			void OnRigidbodyConstruct(entt::registry& registry, entt::entity entity) {
				b2BodyDef def = b2DefaultBodyDef();

				auto& rb = registry.get<Physics::Rigidbody_EntityComponent>(entity);

				switch (rb.m_Type) {
				case Physics::BodyType::Static:    def.type = b2_staticBody; break;
				case Physics::BodyType::Kinematic: def.type = b2_kinematicBody; break;
				case Physics::BodyType::Dynamic:   def.type = b2_dynamicBody; break;
				}

				def.position.x = rb.m_Position.x;
				def.position.y = rb.m_Position.y;

				rb.m_BodyID = b2CreateBody(m_WorldID, &def);

			}

			void OnRigidbodyDestroy(entt::registry& registry, entt::entity entity) {
				auto& rb = registry.get<Physics::Rigidbody_EntityComponent>(entity);
				b2DestroyBody(rb.m_BodyID);
			}

			operator b2WorldId() {
				return m_WorldID;
			}

		private:
			friend class Scene;

			b2WorldId m_WorldID{ b2_nullWorldId };
		};
	}



}