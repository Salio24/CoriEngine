#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <box2cpp/box2cpp.h>
#include "Entity.hpp"
#include "Audio/Track.hpp"
#include "Graphics/Vulkan/Renderer/SceneRenderer.hpp"

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
	namespace Graphics {
		struct RenderObject;
	}
	/**
	 * @brief Anything connected to WorldSystem (ECS) is in this namespace.
	 */
	namespace World {
		class Scene;

		namespace Systems {
			class Transform;
			class Hierarchy;
			class Animation;
			class RenderSync;
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

					struct TransformDirtyForRendererFlag {
						TransformDirtyForRendererFlag() = default;
					private:
						// entt cant create fully empty components
						[[maybe_unused]] bool bober{};
					};

					struct RenderComponentDirtyFlag {
						RenderComponentDirtyFlag() = default;
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
					void SetLocalPosition(const glm::vec3& localPosition) {
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
					void SetLocalScale(const glm::vec3& localScale) {
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
					[[nodiscard]] glm::vec3 GetLocalPosition() const {
						return m_LocalPosition;
					}

					/**
					 * @brief Retries the local scale.
					 * @return Local scale.
					 */
					[[nodiscard]] glm::vec3 GetLocalScale() const {
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
					[[nodiscard]] glm::mat4 GetLocalTransform() const {
						return glm::translate(glm::mat4(1.0f), m_LocalPosition) *
								//glm::rotate(glm::mat4(1.0f), glm::radians(m_LocalRotation)) *
									glm::scale(glm::mat4(1.0f), m_LocalScale);
					}

				private:
					friend Systems::Transform;
					glm::vec3 m_LocalPosition{ 0.0f, 0.0f, 0.0f };
					glm::vec3 m_LocalScale{ 1.0f, 1.0f, 1.0f };

					//use quaternions
					float m_LocalRotation{ 0.0f };
					glm::mat4 m_LastParentTransform{ 1.0f };
					World::Entity m_Owner;
				public:
					glm::mat4 m_WorldTransform{ 1.0f };
					uint8_t m_WorldDepth{ 1 };
				private:
					int16_t m_LocalDepthOffset{ 0 };
					bool m_Detached{ false };
					bool m_DirtyTransform{ true };
					bool m_DirtyDepth{ true };
				};

				struct Rendering {
					Rendering(Core::AssetRef<Graphics::Mesh> mesh, Core::AssetRef<Graphics::Material> material, const glm::vec4& uvOffsets) : m_Mesh(std::move(mesh)), m_Material(std::move(material)), m_UvOffsets(uvOffsets) {}

					void ChangeMesh(Core::AssetRef<Graphics::Mesh> newMesh) {
						m_Mesh = std::move(newMesh);

						m_MeshDirty = true;

						if (!m_Owner.HasComponents<Internal::RenderComponentDirtyFlag>()) {
							m_Owner.AddComponent<Internal::RenderComponentDirtyFlag>();
						}
					}

					void ChangeMaterial(Core::AssetRef<Graphics::Material> newMaterial) {
						m_Material = std::move(newMaterial);

						m_MaterialDirty = true;

						if (!m_Owner.HasComponents<Internal::RenderComponentDirtyFlag>()) {
							m_Owner.AddComponent<Internal::RenderComponentDirtyFlag>();
						}
					}

					void ChangeUVOffsets(const glm::vec4& newUvOffsets) {
						m_UvOffsets = newUvOffsets;

						m_UvOffsetsDirty = true;

						if (!m_Owner.HasComponents<Internal::RenderComponentDirtyFlag>()) {
							m_Owner.AddComponent<Internal::RenderComponentDirtyFlag>();
						}
					}

					[[nodiscard]] Core::AssetRef<Graphics::Mesh> GetMesh() const {
						return m_Mesh;
					}

					[[nodiscard]] Core::AssetRef<Graphics::Material> GetMaterial() const {
						return m_Material;
					}

					[[nodiscard]] glm::vec4 GetUVOffsets() const {
						return m_UvOffsets;
					}

				protected:
					friend Systems::RenderSync;
					Core::Handle<Graphics::RenderObject> m_RenderObjectHandle;
					World::Entity m_Owner;
				private:
					Core::AssetRef<Graphics::Mesh> m_Mesh;
					Core::AssetRef<Graphics::Material> m_Material;
					glm::vec4 m_UvOffsets{ 0.0f, 0.0f, 1.0f, 1.0f };
					bool m_MeshDirty{ true };
					bool m_MaterialDirty{ true };
					bool m_UvOffsetsDirty{ true };
					bool m_NewRegister{ true };
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