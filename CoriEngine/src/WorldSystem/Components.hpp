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
#include "Renderer/Animator/Animation.hpp"
#include "Audio/Sound.hpp"
#include "Audio/Track.hpp"

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
			private:
				friend class Cori::Entity;
				friend class Cori::Scene;
				std::string m_Name;
			};

			struct Tag {
				Tag() = default;
				explicit Tag(const Utility::HashedTag64& tag) : m_Tag(tag) {}

				Utility::HashedTag64 m_Tag;
			};

			struct UUID {
				UUID() = default;
				explicit UUID(const Core::UUID& uuid) : m_UUID(uuid) {}
				explicit UUID(const std::string& uuid_str) : m_UUID(uuid_str) {}

				const Core::UUID m_UUID{};
			};

			struct Hierarchy {
				Hierarchy() = default;
				entt::entity m_Parent {entt::null};
				entt::entity m_FirstChild {entt::null};
				entt::entity m_NextSibling {entt::null};
				entt::entity m_PreviousSibling {entt::null};
			};

			struct Transform {
				Transform() = default;
				Transform(const glm::vec2 localPosition, const uint8_t localLayer) : m_LocalPosition(localPosition), m_LocalDepthOffset(localLayer) {}

				void SetLocalPosition(const glm::vec2 localPosition) {
					if (m_LocalPosition != localPosition) {
						m_LocalPosition = localPosition;
						m_DirtyTransform = true;
					}
				}

				void SetLocalRotation(const float localRotation) {
					if (m_LocalRotation != localRotation) {
						m_LocalRotation = localRotation;
						m_DirtyTransform = true;
					}
				}

				void SetLocalScale(const glm::vec2 localScale) {
					if (m_LocalScale != localScale) {
						m_LocalScale = localScale;
						m_DirtyTransform = true;
					}
				}

				void SetLocalDepth(const uint8_t localDepth) {
					if (m_LocalDepthOffset != localDepth) {
						m_LocalDepthOffset = localDepth;
						m_DirtyDepth = true;
					}
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

				[[nodiscard]] uint8_t GetLocalDepthOffset() const {
					return m_LocalDepthOffset;
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
				uint8_t m_WorldDepth{ 1 };
			private:
				uint8_t m_LocalDepthOffset{ 0 };
				bool m_DirtyTransform{ true };
				bool m_DirtyDepth{ true };
			};

			struct ChildCache {
				ChildCache() = default;
				std::unordered_map<std::string, entt::entity> m_Children;
			};

			struct InactiveLocallyFlag {
				InactiveLocallyFlag() = default;
			private:
				// entt cant create fully empty components
				[[maybe_unused]] uint8_t placeholder{ 0 };
			};

			struct InactiveGloballyFlag {
				InactiveGloballyFlag() = default;
			private:
				// entt cant create fully empty components
				[[maybe_unused]] uint8_t placeholder{ 0 };
			};

			struct QuadRenderer {
				QuadRenderer() = default;

				QuadRenderer(const glm::vec2 size, const std::shared_ptr<Texture2D>& texture, const UVs& uvs) : m_HalfSize(size), m_UVs(uvs) {
					SetTexture(texture);
				}

				QuadRenderer(const glm::vec2 size, const std::shared_ptr<Texture2D>& texture, const UVs& uvs, const glm::vec4& tintColor) : m_HalfSize(size), m_UVs(uvs) {
					SetColor(tintColor);
					SetTexture(texture);
				}

				QuadRenderer(const glm::vec2 size, const glm::vec4& color) : m_HalfSize(size), m_FlatColored(true) {
					SetColor(color);
				}

				void SetTexture(const std::shared_ptr<Texture2D>& texture) {
					if (!m_AnimatorBound) {
						if (m_HasSemiTransparency && m_Color.a != 1.0f) {
							m_Texture = texture;
							return;
						}
						m_HasSemiTransparency = texture->HasSemiTransparency();
						m_Texture = texture;
						return;
					}
					CORI_CORE_WARN_TAGGED({"World", "Entity", "Components"}, "Can't set texture for QuadRenderer because an animator is currently bound.");
				}

				std::shared_ptr<Texture2D> GetTexture() const {
					return m_Texture;
				}

				void SetColor(const glm::vec4& color) {
					if (m_HasSemiTransparency && (m_Texture ? m_Texture->HasSemiTransparency() : false)) {
						m_Color = color;
						return;
					}
					m_Color = color;
					if (m_Color.a != 1.0f) {
						m_HasSemiTransparency = true;
					} else {
						m_HasSemiTransparency = false;
					}
				}

				[[nodiscard]] const glm::vec4& GetColor() const {
					return m_Color;
				}

				void SetUVs(const UVs& uvs) {
					if (!m_AnimatorBound) {
						m_UVs = uvs;
						return;
					}
					CORI_CORE_WARN_TAGGED({"World", "Entity", "Components"}, "Can't set UVs for QuadRenderer because an animator is currently bound.");
				}

				[[nodiscard]] UVs GetUVs() const {
					return m_UVs;
				}

				//void SetSize(const glm::vec2& size) {
				//	if (!m_AnimatorBound) {
				//		m_Size = size;
				//		return;
				//	}
				//	CORI_CORE_WARN_TAGGED({"World", "Entity", "Components"}, "Can't set size for QuadRenderer because an animator is currently bound.");
				//}

				//glm::vec2 GetSize() const {
				//	return m_Size;
				//}

				bool GetSemiTransparencyState() const {
					return m_HasSemiTransparency;
				}

				glm::vec2 m_HalfSize{ 0.0f };

			protected:
				friend class QuadAnimator;
				UVs m_UVs{};
				std::shared_ptr<Texture2D> m_Texture{ nullptr };
			private:
				glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
			public:
				bool m_Visible{ true };
				bool m_FlatColored{ false };
				bool m_FlipX{ false };
				bool m_FlipY{ false };
			private:
				bool m_HasSemiTransparency{ false };
				bool m_AnimatorBound{ false };
			};

			struct AudioSource {
				AudioSource() = default;

				void AddTrack(const std::string& name) {
					std::shared_ptr<Audio::Track> track = Audio::Track::Create(name);
					m_AudioTracks.insert({name, track});
				}

				void RemoveTrack(const std::string& name) {
					if (m_AudioTracks.contains(name)) {
						m_AudioTracks.erase(name);
					}
				}

				// use expected
				std::weak_ptr<Audio::Track> GetTrack(const std::string& name) {
					return m_AudioTracks.at(name);
				}

			private:
				std::unordered_map<std::string, std::shared_ptr<Audio::Track>> m_AudioTracks;
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