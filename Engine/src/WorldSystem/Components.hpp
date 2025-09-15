#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <box2cpp/box2cpp.h>
#include "Entity.hpp"
#include "Graphics/Texture.hpp"
#include <glm/gtx/matrix_transform_2d.hpp>
#include <utility>
#include "Utility/HashedTag.hpp"
#include "Audio/Track.hpp"

namespace Cori {
	namespace Physics {
		struct BodyUserData {
			static constexpr auto in_place_delete = true;

			BodyUserData() = default;
			explicit BodyUserData(const Cori::World::Entity& entity) : m_Entity(entity) {}
			Cori::World::Entity m_Entity;
		};
	}
}

namespace Cori {
	namespace World {
		class Scene;
		namespace Components {
			namespace Entity {
				struct Name {
					Name() = default;
				private:
					friend World::Entity;
					friend World::Scene;
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

				struct DirtyTransformFlag {
					DirtyTransformFlag() = default;
				private:
					// entt cant create fully empty components
					[[maybe_unused]] uint8_t placeholder{ 0 };
				};

				struct Transform {
					Transform() = default;
					explicit Transform(const World::Entity& owner) : m_Owner(owner) {}

					void SetLocalPosition(const glm::vec2 localPosition) {
						if (m_LocalPosition != localPosition) {
							m_LocalPosition = localPosition;
							m_DirtyTransform = true;

							entt::entity root = m_Owner.GetRawEntity();
							entt::entity currentParent = m_Owner.GetComponents<Hierarchy>().m_Parent;
							while (m_Owner.GetRawHandle().registry()->valid(currentParent)) {
								root = currentParent;
								currentParent = m_Owner.GetRawHandle().registry()->get<Hierarchy>(root).m_Parent;
							}

							if (!m_Owner.GetRawHandle().registry()->all_of<DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<DirtyTransformFlag>(root);
							}
						}
					}

					void SetLocalRotation(const float localRotation) {
						if (m_LocalRotation != localRotation) {
							m_LocalRotation = localRotation;
							m_DirtyTransform = true;

							entt::entity root = m_Owner.GetRawEntity();
							entt::entity currentParent = m_Owner.GetComponents<Hierarchy>().m_Parent;
							while (m_Owner.GetRawHandle().registry()->valid(currentParent)) {
								root = currentParent;
								currentParent = m_Owner.GetRawHandle().registry()->get<Hierarchy>(root).m_Parent;
							}

							if (!m_Owner.GetRawHandle().registry()->all_of<DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<DirtyTransformFlag>(root);
							}
						}
					}

					void SetLocalScale(const glm::vec2 localScale) {
						if (m_LocalScale != localScale) {
							m_LocalScale = localScale;
							m_DirtyTransform = true;

							entt::entity root = m_Owner.GetRawEntity();
							entt::entity currentParent = m_Owner.GetComponents<Hierarchy>().m_Parent;
							while (m_Owner.GetRawHandle().registry()->valid(currentParent)) {
								root = currentParent;
								currentParent = m_Owner.GetRawHandle().registry()->get<Hierarchy>(root).m_Parent;
							}

							if (!m_Owner.GetRawHandle().registry()->all_of<DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<DirtyTransformFlag>(root);
							}
						}
					}

					void SetLocalDepth(const int16_t localDepth) {
						if (m_LocalDepthOffset != localDepth) {
							m_LocalDepthOffset = localDepth;
							m_DirtyDepth = true;

							entt::entity root = m_Owner.GetRawEntity();
							entt::entity currentParent = m_Owner.GetComponents<Hierarchy>().m_Parent;
							while (m_Owner.GetRawHandle().registry()->valid(currentParent)) {
								root = currentParent;
								currentParent = m_Owner.GetRawHandle().registry()->get<Hierarchy>(root).m_Parent;
							}

							if (!m_Owner.GetRawHandle().registry()->all_of<DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<DirtyTransformFlag>(root);
							}
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

					[[nodiscard]] int16_t GetLocalDepthOffset() const {
						return m_LocalDepthOffset;
					}

					void SetDetachedState(const bool state) {
						m_Detached = state;
					}

					[[nodiscard]] bool GetDetachedState() const {
						return m_Detached;
					}

					[[nodiscard]] glm::mat3 GetLocalTransform() const {
						return glm::translate(glm::mat3(1.0f), m_LocalPosition) *
								glm::rotate(glm::mat3(1.0f), glm::radians(m_LocalRotation)) *
									glm::scale(glm::mat3(1.0f), m_LocalScale);
					}

				private:
					friend World::Scene;
					glm::vec2 m_LocalPosition{ 0.0f, 0.0f };
					glm::vec2 m_LocalScale{ 1.0f, 1.0f };
					float m_LocalRotation{ 0.0f };
					glm::mat3 m_LastParentTransform{ 1.0f };
					World::Entity m_Owner;
				public:
					glm::mat3 m_WorldTransform{ 1.0f };
					uint8_t m_WorldDepth{ 1 };
				private:
					int16_t m_LocalDepthOffset{ 0 };
					bool m_Detached{ false };
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

					QuadRenderer(const glm::vec2 halfSize, const std::shared_ptr<Graphics::Texture2D>& texture, const Graphics::UVs& uvs) : m_HalfSize(halfSize), m_UVs(uvs) {
						const auto result = SetTexture(texture);
						if (!result) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadRenderer }, "Failed to fully recreate Quad Renderer. Details: '{}'", result.error().what());
						}
					}

					QuadRenderer(const glm::vec2 halfSize, const std::shared_ptr<Graphics::Texture2D>& texture, const Graphics::UVs& uvs, const glm::vec4& tintColor) : m_HalfSize(halfSize), m_UVs(uvs) {
						const auto result = SetTexture(texture);
						if (!result) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadRenderer }, "Failed to fully recreate Quad Renderer. Details: '{}'", result.error().what());
						}
						SetColor(tintColor);
					}

					QuadRenderer(const glm::vec2 halfSize, const glm::vec4& color) : m_HalfSize(halfSize), m_FlatColored(true) {
						SetColor(color);
					}

					std::expected<void, Core::CoriError<>> SetTexture(const std::shared_ptr<Graphics::Texture2D>& texture) {
						if (!m_AnimatorBound) {
							if (m_HasSemiTransparency && m_Color.a != 1.0f) {
								m_Texture = texture;
								return {};
							}
							m_HasSemiTransparency = texture->HasSemiTransparency();
							m_Texture = texture;
							return {};
						}

						return std::unexpected(Core::CoriError("Can't set Texture2D because Quad Animator is currently bound."));
					}

					[[nodiscard]] std::shared_ptr<Graphics::Texture2D> GetTexture() const {
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

					std::expected<void, Core::CoriError<>> SetUVs(const Graphics::UVs& uvs) {
						if (!m_AnimatorBound) {
							m_UVs = uvs;
							return {};
						}

						return std::unexpected(Core::CoriError("Can't set UVs because Quad Animator is currently bound."));
					}

					[[nodiscard]] Graphics::UVs GetUVs() const {
						return m_UVs;
					}

					std::expected<void, Core::CoriError<>> SetHalfSize(const glm::vec2 halfSize) {
						if (!m_AnimatorBound) {
							m_HalfSize = halfSize;
							return {};
						}
						return std::unexpected(Core::CoriError("Can't set HalfSize because Quad Animator is currently bound."));
					}

					[[nodiscard]] glm::vec2 GetHalfSize() const {
						return m_HalfSize;
					}

					[[nodiscard]] bool GetSemiTransparencyState() const {
						return m_HasSemiTransparency;
					}

				private:
					friend class QuadAnimator;
					friend class QuadAnimatorNew;

					glm::vec2 m_HalfSize{ 0.0f };
					Graphics::UVs m_UVs{};
					std::shared_ptr<Graphics::Texture2D> m_Texture{ nullptr };
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

					std::expected<std::shared_ptr<Audio::Track>, Core::CoriError<>> GetTrack(const std::string& name) {
						if (m_AudioTracks.contains(name)) {
							return m_AudioTracks.at(name);
						}

						return std::unexpected(Core::CoriError(std::format("No audio Track is found with the specified name '{}'", name)));
					}

				private:
					std::unordered_map<std::string, std::shared_ptr<Audio::Track>> m_AudioTracks;
				};

				struct Rigidbody : public Physics::BodyRef {
					Rigidbody(Physics::WorldRef world, const std::derived_from<b2BodyDef> auto& def, World::Entity& owner) : Physics::BodyRef{world.CreateBody(Physics::DestroyWithParent, def)} {
						if (def.type == b2_kinematicBody || def.type == b2_dynamicBody) {
							if (owner.IsValid()) {
								auto& ud = owner.AddComponent<Physics::BodyUserData>(owner);
								SetUserData(&ud);
							}
						}
					}
					~Rigidbody() { if (IsValid()) { Destroy(); } }
				};
			}
		}
	}
}