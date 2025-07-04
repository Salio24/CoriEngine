#pragma once
#include <box2cpp/box2cpp.h>
#include "Entity.hpp"
#include "EventSystem/Event.hpp"
#include "Renderer/Texture.hpp"
#include "StateSystem/StateMachine.hpp"
#include "Renderer/CameraComponent.hpp"

namespace Cori {
	namespace Physics {
		struct BodyUserData {
			static constexpr auto in_place_delete = true;

			BodyUserData() = default;
			BodyUserData(const Cori::Entity& entity) : m_Entity(entity) {}
			Cori::Entity m_Entity;
		};
	}
}

namespace Cori {
	namespace Components {
		namespace Entity {
			struct Name {
				std::string m_Name;
				Name() = default;
				Name(const std::string& name) : m_Name(name) {}
			};


			// add an ability to add multiple plains to an entity
			// combing sprite and render component into one
			struct Render {
				glm::vec2 m_Position{ 0.0f, 0.0f };
				glm::vec2 m_Size{ 0.0f, 0.0f };
				float m_Layer{ 0.0f };
				bool m_Textured{ true };
				bool m_Visible{ true };
				bool m_Flipped{ false };
				bool m_SemiTransparency{ false };
				glm::vec4 m_TintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
				std::shared_ptr<Texture2D> m_Texture{ nullptr };
				UVs m_UVs{};

				Render() = default;
				Render(const glm::vec2& position, const glm::vec2& size, float layer = 5.0f, bool textured = true, bool visible = true)
					: m_Position(position), m_Size(size), m_Layer(layer), m_Textured(textured), m_Visible(visible) {}
			};

			struct Sprite {
				std::shared_ptr<Texture2D> m_Texture{ nullptr };
				UVs m_UVs;
				Sprite() = default;
				Sprite(const std::shared_ptr<Texture2D>& texture, const UVs& uvs = {})
					: m_Texture(texture), m_UVs(uvs) {
				}
			};

			struct Rigidbody : public Physics::BodyRef {
				Rigidbody(Physics::WorldRef world, const std::derived_from<b2BodyDef> auto& def, Cori::Entity& owner) : Physics::BodyRef{world.CreateBody(Physics::DestroyWithParent, def)} {
					if (def.type == b2_kinematicBody || def.type == b2_dynamicBody) {
						if (owner.IsValid()) {
							auto& ud = owner.AddComponent<Cori::Physics::BodyUserData>(owner);
							SetUserData(&ud);
						}
					}
				}
				~Rigidbody() { if (IsValid()) { Destroy(); } }
			};

			struct Spawnpoint {
				glm::vec2 m_Spawnpoint{ 0.0f, 0.0f };
				Spawnpoint() = default;
				Spawnpoint(const glm::vec2& point) 
					: m_Spawnpoint(point) {}
			};

			struct StateMachine {
				StateMachine(Cori::Entity owner) : m_StateMachine(owner) {}

				template<typename S, typename ... Args>
				void Register(Args&&... args) {
					m_StateMachine.RegisterState<S>(std::forward<Args>(args)...);
				}

				template<typename S>
				void SetState() {
					m_StateMachine.ChangeState<S>();
				}

				template<typename S>
				void SetStateIfNotInState() {
					if (!m_StateMachine.IsInState<S>()) {
						m_StateMachine.ChangeState<S>();
					}
				}

				template<typename S>
				bool IsInState() const {
					return m_StateMachine.IsInState<S>();
				}

				State* GetCurrentState() const {
					return m_StateMachine.GetCurrentState();
				}


			protected:

				void Update(float timeStep) {
					m_StateMachine.Update(timeStep);
				}

				friend class Scene;

			private:
				Cori::StateMachine m_StateMachine;
			};
		}
	}
}