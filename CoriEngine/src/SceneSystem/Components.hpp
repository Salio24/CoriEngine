#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <box2cpp/box2cpp.h>
#include "Entity.hpp"
#include "EventSystem/Event.hpp"
#include "glm/gtx/type_trait.hpp"
#include "Renderer/Texture.hpp"
#include "StateSystem/StateMachine.hpp"
#include "Renderer/CameraComponent.hpp"
#include "Core/Utility/TemplateUtils.hpp"
#include <glm/gtx/matrix_transform_2d.hpp>
#include "Core/Utility/HashedTag.hpp"

namespace Cori {
	namespace Physics {
		struct BodyUserData {
			static constexpr auto in_place_delete = true;

			BodyUserData() = default;
			explicit BodyUserData(const Cori::Entity& entity) : m_Entity(entity) {}
			Cori::Entity m_Entity;
		};
	}
}

namespace Cori {
	namespace Components {
		namespace Entity {
			struct Name {
				Name() = default;
				explicit Name(const std::string& name) : m_Name(name) {}
			//private:
				friend class Cori::Entity;
				std::string m_Name;
			};

			struct TagComponent {
				TagComponent() = default;
				explicit TagComponent(const Utils::HashedTag64& tag) : m_Tag(tag) {}

				Utils::HashedTag64 m_Tag;
			};

			struct UUIDComponent {
				UUIDComponent() = default;
				explicit UUIDComponent(const Core::UUID& uuid) : m_UUID(uuid) {}
				explicit UUIDComponent(const std::string& uuid_str) : m_UUID(uuid_str) {}

				const Core::UUID m_UUID{};
			};

			struct HierarchyComponent {
				HierarchyComponent() = default;
				entt::entity m_Parent {entt::null};
				entt::entity m_FirstChild {entt::null};
				entt::entity m_NextSibling {entt::null};
				entt::entity m_PreviousSibling {entt::null};
			};

			struct TransformComponent {
				TransformComponent() = default;
				TransformComponent(const glm::vec2 localPosition, const uint8_t localLayer) : m_LocalPosition(localPosition), m_LocalLayer(localLayer) {}

				void SetLocalPosition(const glm::vec2 localPosition) {
					m_LocalPosition = localPosition;
					m_DirtyTransform = true;
				}

				void SetLocalRotation(const float localRotation) {
					m_LocalRotation = localRotation;
					m_DirtyTransform = true;
				}

				void SetLocalScale(const glm::vec2 localScale) {
					m_LocalScale = localScale;
					m_DirtyTransform = true;
				}

				void SetLocalLayer(const uint8_t localLayer) {
					m_LocalLayer = localLayer;
					m_DirtyLayer = true;
				}

				[[nodiscard]] glm::vec2 GetLocalPosition() const {
					return m_LocalPosition;
				}

				[[nodiscard]] glm::vec2 GetLocalScale() const {
					return m_LocalScale;
				}

				[[nodiscard]] float GetLocalRotation() const {
					return m_LocalRotation;
				}

				[[nodiscard]] uint8_t GetLocalLayer() const {
					return m_LocalLayer;
				}

				[[nodiscard]] glm::mat3 GetLocalTransform() const {
					return glm::translate(glm::mat3(1.0f), m_LocalPosition) *
							glm::rotate(glm::mat3(1.0f), glm::radians(m_LocalRotation)) *
								glm::scale(glm::mat3(1.0f), m_LocalScale);
				}

			private:
				friend class Cori::Scene;
				glm::vec2 m_LocalPosition{ 0.0f, 0.0f };
				glm::vec2 m_LocalScale{ 1.0f, 1.0f };
				float m_LocalRotation{ 0.0f };
			public:
				glm::mat3 m_WorldTransform{ 1.0f };
				uint8_t m_WorldLayer{ 1 };
			private:
				uint8_t m_LocalLayer{ 0 };
				bool m_DirtyTransform{ true };
				bool m_DirtyLayer{ true };
			};

			struct ChildCacheComponent {
				ChildCacheComponent() = default;
				std::unordered_map<std::string, entt::entity> m_Children;
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
							auto& ud = owner.AddComponent<Physics::BodyUserData>(owner);
							SetUserData(&ud);
						}
					}
				}
				~Rigidbody() { if (IsValid()) { Destroy(); } }
			};

			struct Spawnpoint {
				Spawnpoint() = default;
				explicit Spawnpoint(const glm::vec2& point)
					: m_Spawnpoint(point) {}
				glm::vec2 m_Spawnpoint{ 0.0f, 0.0f };
			};

			struct StateMachine {
				explicit StateMachine(Cori::Entity owner) : m_StateMachine(owner) {}

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
				[[nodiscard]] bool IsInState() const {
					return m_StateMachine.IsInState<S>();
				}

				[[nodiscard]] State* GetCurrentState() const {
					return m_StateMachine.GetCurrentState();
				}


			protected:
				friend class Cori::Scene;

				void Update(float timeStep) {
					m_StateMachine.Update(timeStep);
				}


			private:
				Cori::StateMachine m_StateMachine;
			};
		}
	}
}