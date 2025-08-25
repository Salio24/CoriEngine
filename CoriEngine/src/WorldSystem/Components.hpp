#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <box2cpp/box2cpp.h>
#include "Entity.hpp"
#include "Graphics/Texture.hpp"
#include "StateSystem/StateMachine.hpp"
#include <glm/gtx/matrix_transform_2d.hpp>
#include <utility>
#include "Core/Utility/HashedTag.hpp"
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
			protected:
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
				explicit UUID(Core::UUID  uuid) : m_UUID(std::move(uuid)) {}
				explicit UUID(const std::string& uuid_str) : m_UUID(uuid_str) {}

				const Core::UUID m_UUID{};
			};

			struct Hierarchy {
				Hierarchy() = default;
				entt::entity m_Parent { entt::null };
				entt::entity m_FirstChild { entt::null };
				entt::entity m_NextSibling { entt::null };
				entt::entity m_PreviousSibling { entt::null };
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
					const auto result = SetTexture(texture);
					if (!result) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadRenderer }, "Failed to fully recreate QuadAnimator. Details: '{}'", result.error().what());
					}
				}

				QuadRenderer(const glm::vec2 size, const std::shared_ptr<Texture2D>& texture, const UVs& uvs, const glm::vec4& tintColor) : m_HalfSize(size), m_UVs(uvs) {
					const auto result = SetTexture(texture);
					if (!result) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadRenderer }, "Failed to fully recreate QuadAnimator. Details: '{}'", result.error().what());
					}
					SetColor(tintColor);
				}

				QuadRenderer(const glm::vec2 size, const glm::vec4& color) : m_HalfSize(size), m_FlatColored(true) {
					SetColor(color);
				}

				[[nodiscard]] std::expected<void, CoriError<>> SetTexture(const std::shared_ptr<Texture2D>& texture) {
					if (!m_AnimatorBound) {
						if (m_HasSemiTransparency && m_Color.a != 1.0f) {
							m_Texture = texture;
							return {};
						}
						m_HasSemiTransparency = texture->HasSemiTransparency();
						m_Texture = texture;
						return {};
					}

					return std::unexpected(CoriError("Can't set Texture2D because QuadAnimator is currently bound."));
				}

				[[nodiscard]] std::shared_ptr<Texture2D> GetTexture() const {
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

				[[nodiscard]] std::expected<void, CoriError<>> SetUVs(const UVs& uvs) {
					if (!m_AnimatorBound) {
						m_UVs = uvs;
						return {};
					}

					return std::unexpected(CoriError("Can't set UVs because QuadAnimator is currently bound."));
				}

				[[nodiscard]] UVs GetUVs() const {
					return m_UVs;
				}

				[[nodiscard]] std::expected<void, CoriError<>> SetHalfSize(const glm::vec2 halfSize) {
					if (!m_AnimatorBound) {
						m_HalfSize = halfSize;
						return {};
					}
					return std::unexpected(CoriError("Can't set HalfSize because QuadAnimator is currently bound."));
				}

				[[nodiscard]] glm::vec2 GetHalfSize() const {
					return m_HalfSize;
				}

				[[nodiscard]] bool GetSemiTransparencyState() const {
					return m_HasSemiTransparency;
				}

			protected:
				friend class QuadAnimator;
				glm::vec2 m_HalfSize{ 0.0f };
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
			protected:
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
		}
	}
}