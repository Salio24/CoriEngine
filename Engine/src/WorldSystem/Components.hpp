#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <box2cpp/box2cpp.h>
#include "Entity.hpp"
#include "Graphics/Texture.hpp"
#include <glm/gtx/matrix_transform_2d.hpp>
#include <utility>
#include "Audio/Track.hpp"

namespace Cori {
	namespace Physics {
		struct BodyUserData {
			static constexpr auto in_place_delete = true;

			BodyUserData() = default;
			Cori::World::Entity m_Entity;
		};
	}
}

namespace Cori {
	/**
	 * @brief Anything connected to WorldSystem (ECS) is in this namespace.
	 */
	namespace World {
		class Scene;

		namespace Systems {
			class Transform;
			class Hierarchy;
			class Animation;
		}

		/**
		 * @brief Components that are used with the WorldSystem (ECS).
		 */
		namespace Components {
			/**
			 * @brief Components designed to be used with entities.
			 */
			namespace Entity {
				namespace Internal {
					/**
					 * @brief Flags an Entity transform for recalculation. Only for internal usage!
					 * @warning Don't add or remove this component manually!
					 */
					struct DirtyTransformFlag {
						DirtyTransformFlag() = default;

					private:
						// entt cant create fully empty components
						[[maybe_unused]] bool bober{};
					};
				}

				/**
				 * @brief Every Entity by default has a name component, it holds a non-unique entity name. Just holds the data, use Entity methods to manipulate the name of the Entity.
				 */
				struct Name {
					Name() = default;
				private:
					friend World::Entity;
					friend World::Scene;
					std::string m_Name;
				};

				/**
				 * @brief Every Entity by default has a UUID component, mostly unused for now.
				 */
				struct UUID {
					UUID() = default;
					explicit UUID(Core::UUID  uuid) : m_UUID(std::move(uuid)) {}
					explicit UUID(const std::string& uuid_str) : m_UUID(uuid_str) {}

					UUID(const UUID&) = delete;
					UUID& operator=(const UUID&) = delete;
					UUID(UUID&&) = delete;
					UUID& operator=(UUID&&) = delete;

					const Core::UUID m_UUID{};
				};

				/**
				 * @brief Every Entity has a hierarchy component by default, it's essential for entity hierarchy system.
				 */
				struct Hierarchy {
					Hierarchy() = default;

					Hierarchy(const Hierarchy&) = delete;
					Hierarchy& operator=(const Hierarchy&) = delete;
					Hierarchy(Hierarchy&&) = delete;
					Hierarchy& operator=(Hierarchy&&) = delete;

				private:
					friend World::Entity;
					friend Systems::Transform;
					friend Systems::Hierarchy;
					friend struct Transform;
					entt::entity m_Parent { entt::null };
					entt::entity m_FirstChild { entt::null };
					entt::entity m_NextSibling { entt::null };
					entt::entity m_PreviousSibling { entt::null };
				};

				/**
				 * @brief Every Entity has a transform component by default, essential for rendering.
				 */
				struct Transform {
					Transform() = default;

					Transform(const Transform&) = delete;
					Transform& operator=(const Transform&) = delete;
					Transform(Transform&&) = delete;
					Transform& operator=(Transform&&) = delete;

					/**
					 * @brief Changes the local position of the Entity.
					 * @param localPosition Local position to set.
					 * @detals Local position is the position relative to the Entity parent transform (I mean matrix transform in this context), if an Entity doesn't have a parent, and thus is a root entity, the local position is its world position.
					 */
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

							if (!m_Owner.GetRawHandle().registry()->all_of<Internal::DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<Internal::DirtyTransformFlag>(root);
							}
						}
					}

					/**
					 * @brief Changes the local rotation of the Entity.
					 * @param localRotation Local rotation to set.
					 * @detals Local rotation is the rotation relative to the Entity parent transform (matrix), if an Entity doesn't have a parent, and thus is a root entity, the local rotation is its world rotation.
					 */
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

							if (!m_Owner.GetRawHandle().registry()->all_of<Internal::DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<Internal::DirtyTransformFlag>(root);
							}
						}
					}

					/**
					 * @brief Changes the local scale of the Entity.
					 * @param localScale Local scale to set.
					 * @detals Local scale is the scale relative to the Entity parent transform (matrix), if an Entity doesn't have a parent, and thus is a root entity, the local scale is its world scale.
					 */
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

							if (!m_Owner.GetRawHandle().registry()->all_of<Internal::DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<Internal::DirtyTransformFlag>(root);
							}
						}
					}

					/**
					 * @brief Changes the local depth of the Entity.
					 * @param localDepth Local depth to set.
					 * @detals Local depth is the depth relative to the Entity parent depth, if an Entity doesn't have a parent, and thus is a root entity, the local depth is its world depth.
					 */
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

							if (!m_Owner.GetRawHandle().registry()->all_of<Internal::DirtyTransformFlag>(root)) {
								m_Owner.GetRawHandle().registry()->emplace<Internal::DirtyTransformFlag>(root);
							}
						}
					}

					/**
					 * @brief Retries the local position.
					 * @return Local position.
					 */
					[[nodiscard]] glm::vec2 GetLocalPosition() const {
						return m_LocalPosition;
					}

					/**
					 * @brief Retries the local scale.
					 * @return Local scale.
					 */
					[[nodiscard]] glm::vec2 GetLocalScale() const {
						return m_LocalScale;
					}

					/**
					 * @brief Retries the local rotation.
					 * @return Local rotation.
					 */
					[[nodiscard]] float GetLocalRotation() const {
						return m_LocalRotation;
					}

					/**
					 * @brief Retries the local depth.
					 * @return Local depth.
					 */
					[[nodiscard]] int16_t GetLocalDepthOffset() const {
						return m_LocalDepthOffset;
					}

					/**
					 * @brief Sets the detach state.
					 * @param state State to set.
					 * @details When you set transform detach state to true, it is temporary detached form its parent transform, you can still change local positions (position, rotation, scale, depth) relative to the parent transform value at the time of detachment.
					 */
					void SetDetachedState(const bool state) {
						m_Detached = state;
					}

					/**
					 * @brief Gets the current state of detachment.
					 * @return Detachment state.
					 */
					[[nodiscard]] bool GetDetachedState() const {
						return m_Detached;
					}

					/**
					 * @brief Calculates a local transform (matrix) of the Entity, combines all the positions (position, rotation, scale, depth) data, but does take into account parent transform.
					 * @return Calculated local transform (matrix).
					 */
					[[nodiscard]] glm::mat3 GetLocalTransform() const {
						return glm::translate(glm::mat3(1.0f), m_LocalPosition) *
								glm::rotate(glm::mat3(1.0f), glm::radians(m_LocalRotation)) *
									glm::scale(glm::mat3(1.0f), m_LocalScale);
					}

				private:
					friend Systems::Transform;
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

				/**
				 * @brief Every Entity has a ChildCache component by default, it's essential for entity ChildCache system.
				 */
				struct ChildCache {
					ChildCache() = default;

					ChildCache(const ChildCache&) = delete;
					ChildCache& operator=(const ChildCache&) = delete;
					ChildCache(ChildCache&&) = delete;
					ChildCache& operator=(ChildCache&&) = delete;

				private:
					friend World::Entity;
					friend Systems::Hierarchy;
					struct TransparentHash {
						using is_transparent = void;
						size_t operator()(std::string_view sv) const noexcept {
							return std::hash<std::string_view>{}(sv);
						}
					};

					struct TransparentEqual {
						using is_transparent = void;
						bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
							return lhs == rhs;
						}
					};

					std::unordered_map<std::string, entt::entity, TransparentHash, TransparentEqual> m_Children;
				};

				/**
				 * @brief Entity acquires this flag when is disabled explicitly.
				 * @warning Don't add or remove this component manually!
				 */
				struct InactiveLocallyFlag {
					InactiveLocallyFlag() = default;
				private:
					// entt cant create fully empty components
					[[maybe_unused]] bool pingvin{};
				};

				/**
				 * @brief Entity acquires this flag when is disabled explicitly or its parent Entity is disabled.
				 * @warning Don't add or remove this component manually!
				 */
				struct InactiveGloballyFlag {
					InactiveGloballyFlag() = default;
				private:
					// entt cant create fully empty components
					[[maybe_unused]] bool homik{};
				};

				/**
				 * @brief Renders a quad the transform.
				 * @details Transform position is the center of the quad, that's why we need half size.
				 */
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

					QuadRenderer(const QuadRenderer&) = delete;
					QuadRenderer& operator=(const QuadRenderer&) = delete;
					QuadRenderer(QuadRenderer&&) = delete;
					QuadRenderer& operator=(QuadRenderer&&) = delete;

					/**
					 * @brief Changes the texture that is used to render the quad.
					 * @param texture Texture to change to.
					 * @return Expected object with void on success or CoriError<> on failure.
					 * @note You can't change a texture if an Entity has QuadAnimator component.
					 */
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

					/**
					 * @brief Returns a const reference to the currently active texture.
					 * @return Const reference to the active texture.
					 */
					[[nodiscard]] const std::shared_ptr<Graphics::Texture2D>& GetTexture() const {
						return m_Texture;
					}

					/**
					 * @brief Changes the tint color of the quad.
					 * @param color Color to change to.
					 */
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

					/**
					 * @brief Gets a const reference to the current tint color of the quad.
					 * @return Const reference to the current color.
					 */
					[[nodiscard]] const glm::vec4& GetColor() const {
						return m_Color;
					}

					/**
					 * @brief Changes the current UVs that are used when sampling fom the texture.
					 * @param uvs UVs to set.
					 * @return Expected object with void on success or CoriError<> on failure.
					 * @note You can't change a UVs if an Entity has QuadAnimator component.
					 */
					std::expected<void, Core::CoriError<>> SetUVs(const Graphics::UVs& uvs) {
						if (!m_AnimatorBound) {
							m_UVs = uvs;
							return {};
						}

						return std::unexpected(Core::CoriError("Can't set UVs because Quad Animator is currently bound."));
					}

					/**
					 * @brief Gets a const reference to the current UVs of the quad.
					 * @return Const reference to the current UVs.
					 */
					[[nodiscard]] const Graphics::UVs& GetUVs() const {
						return m_UVs;
					}

					/**
					 * @brief Changes the current half size that are used when drawing the quad.
					 * @param halfSize Half size to set.
					 * @return Expected object with void on success or CoriError<> on failure.
					 * @note You can't change a half size if an Entity has QuadAnimator component.
					 */
					std::expected<void, Core::CoriError<>> SetHalfSize(const glm::vec2 halfSize) {
						if (!m_AnimatorBound) {
							m_HalfSize = halfSize;
							return {};
						}
						return std::unexpected(Core::CoriError("Can't set HalfSize because Quad Animator is currently bound."));
					}

					/**
					 * @brief Gets the current quad half size.
					 * @return Current quad half size.
					 */
					[[nodiscard]] glm::vec2 GetHalfSize() const {
						return m_HalfSize;
					}

					/**
					 * @brief Checks if a quad needs to be processed with transparency in mind or no.
					 * @return Quad semi transparency state.
					 */
					[[nodiscard]] bool GetSemiTransparencyState() const {
						return m_HasSemiTransparency;
					}

				private:
					friend class QuadAnimator;
					friend Systems::Animation;

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

				/**
				 * @brief Allows an Entity to emit a sound.
				 */
				struct AudioSource {
					AudioSource() = default;

					AudioSource(const AudioSource&) = delete;
					AudioSource& operator=(const AudioSource&) = delete;
					AudioSource(AudioSource&&) = delete;
					AudioSource& operator=(AudioSource&&) = delete;

					/**
					 * @brief Adds a Track to the AudioSource cache.
					 * @param name Name of the track, will be later used to retrieve the created track from the AudioSource cache.
					 * @return Shared pointer to the created Track object.
					 */
					std::shared_ptr<Audio::Track> AddTrack(const std::string& name) {
						std::shared_ptr<Audio::Track> track = Audio::Track::Create(name);
						m_AudioTracks.insert({name, track});
						return track;
					}

					/**
					 * @brief Adds a Track to the AudioSource cache.
					 * @param name Name of the track, will be later used to retrieve the created track from the AudioSource cache.
					 * @return Shared pointer to the created Track object.
					 */
					std::shared_ptr<Audio::Track> AddTrack(const char* name) {
						std::shared_ptr<Audio::Track> track = Audio::Track::Create(name);
						m_AudioTracks.insert({name, track});
						return track;
					}

					/**
					 * @brief Removes a Track from the AudioSource, and deletes it if it is not referenced anywhere.
					 * @param name Name of the Track remove.
					 * @note If a track with the specified name is not found in AudioSource cache, nothing will happen.
					 */
					void RemoveTrack(const std::string& name) {
						if (m_AudioTracks.contains(name)) {
							m_AudioTracks.erase(name);
						}
					}

					/**
					 * @brief Removes a Track from the AudioSource, and deletes it if it is not referenced anywhere.
					 * @param name Name of the Track remove.
					 * @note If a track with the specified name is not found in AudioSource cache, nothing will happen.
					 */
					void RemoveTrack(const char* name) {
						if (m_AudioTracks.contains(name)) {
							m_AudioTracks.erase(name);
						}
					}

					/**
					 * @brief Retries a Track from the AudioSource cache.
					 * @param name Name of the Track to retrieve.
					 * @return Expected object with the shared pointer to the requested Track on success, or CoriError on failure.
					 */
					std::expected<std::shared_ptr<Audio::Track>, Core::CoriError<>> GetTrack(const std::string& name) {
						if (m_AudioTracks.contains(name)) {
							return m_AudioTracks.at(name);
						}

						return std::unexpected(Core::CoriError(std::format("No audio Track is found with the specified name '{}'", name)));
					}

					/**
					 * @brief Retries a Track from the AudioSource cache.
					 * @param name Name of the Track to retrieve.
					 * @return Expected object with the shared pointer to the requested Track on success, or CoriError on failure.
					 */
					std::expected<std::shared_ptr<Audio::Track>, Core::CoriError<>> GetTrack(const char* name) {
						if (m_AudioTracks.contains(name)) {
							return m_AudioTracks.at(name);
						}

						return std::unexpected(Core::CoriError(std::format("No audio Track is found with the specified name '{}'", name)));
					}

				private:
					struct TransparentHash {
						using is_transparent = void;
						size_t operator()(std::string_view sv) const noexcept {
							return std::hash<std::string_view>{}(sv);
						}
					};

					struct TransparentEqual {
						using is_transparent = void;
						bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
							return lhs == rhs;
						}
					};

					std::unordered_map<std::string, std::shared_ptr<Audio::Track>, TransparentHash, TransparentEqual> m_AudioTracks;
				};

				/**
				 * @brief A physics rigid body.
				 * @details Cori engine doesn't have a native physics engine and uses Box2D, so refer to Box2D docs 'https://box2d.org/' for any details on physics.
				 * \n All the engine does is provide a convenient C++ API for it, as Box2D is a C project and the default API is not really convenient in C++ environment.
				 * \n Big thanks HolyBlackCat for: 'https://github.com/HolyBlackCat/box2cpp/tree/master'
				 */
				struct RigidBody : Physics::BodyRef {
					/**
					 * @brief Creates a RigidBody.
					 * @param world World to create the RigidBody in.
					 * @param def Parameters to create a RigidBody with.
					 */
					RigidBody(Physics::WorldRef world, const std::derived_from<b2BodyDef> auto& def) : Physics::BodyRef{world.CreateBody(Physics::DestroyWithParent, def)} {}

					~RigidBody() { if (IsValid()) { Destroy(); } }

					RigidBody(const RigidBody&) = delete;
					RigidBody& operator=(const RigidBody&) = delete;
					RigidBody(RigidBody&&) = delete;
					RigidBody& operator=(RigidBody&&) = delete;
				};
			}
		}
	}
}