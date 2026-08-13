#include "Components.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				std::shared_ptr<Audio::Track> AudioSource::AddTrack(const std::string& name) {
					std::shared_ptr<Audio::Track> track = Audio::Track::Create(name);
					m_AudioTracks.insert({name, track});
					return track;
				}

				std::shared_ptr<Audio::Track> AudioSource::AddTrack(const char* name) {
					std::shared_ptr<Audio::Track> track = Audio::Track::Create(name);
					m_AudioTracks.insert({name, track});
					return track;
				}

				void AudioSource::RemoveTrack(const std::string& name) {
					if (m_AudioTracks.contains(name)) {
						m_AudioTracks.erase(name);
					}
				}

				void AudioSource::RemoveTrack(const char* name) {
					if (m_AudioTracks.contains(name)) {
						m_AudioTracks.erase(name);
					}
				}

				std::expected<std::shared_ptr<Audio::Track>, Core::CoriError<>> AudioSource::GetTrack(const std::string& name) {
					if (m_AudioTracks.contains(name)) {
						return m_AudioTracks.at(name);
					}

					return std::unexpected(Core::CoriError(std::format("No audio Track is found with the specified name '{}'", name)));
				}

				std::expected<std::shared_ptr<Audio::Track>, Core::CoriError<>> AudioSource::GetTrack(const char* name) {
					if (m_AudioTracks.contains(name)) {
						return m_AudioTracks.at(name);
					}

					return std::unexpected(Core::CoriError(std::format("No audio Track is found with the specified name '{}'", name)));
				}

				void Rendering::ChangeMesh(Core::AssetRef<Graphics::Mesh> newMesh) {
					m_Mesh = std::move(newMesh);

					m_MeshDirty = true;

					if (!m_Owner.HasComponents<Internal::RenderComponentDirtyFlag>()) {
						m_Owner.AddComponent<Internal::RenderComponentDirtyFlag>();
					}
				}

				void Rendering::ChangeMaterial(Core::AssetRef<Graphics::Material> newMaterial) {
					m_Material = std::move(newMaterial);

					m_MaterialDirty = true;

					if (!m_Owner.HasComponents<Internal::RenderComponentDirtyFlag>()) {
						m_Owner.AddComponent<Internal::RenderComponentDirtyFlag>();
					}
				}

				void Rendering::ChangeUVOffsets(const glm::vec4& newUvOffsets) {
					m_UvOffsets = newUvOffsets;

					m_UvOffsetsDirty = true;

					if (!m_Owner.HasComponents<Internal::RenderComponentDirtyFlag>()) {
						m_Owner.AddComponent<Internal::RenderComponentDirtyFlag>();
					}
				}

				void Transform::SetLocalPosition(const glm::vec3& localPosition) {
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

				void Transform::SetLocalRotation(const float localRotation, const glm::vec3& axis) {
					if (m_LocalRotation != localRotation || m_LocalRotationAxis != axis) {
						m_LocalRotation = localRotation;
						m_LocalRotationAxis = axis;
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

				void Transform::SetLocalScale(const glm::vec3& localScale) {
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

				void Transform::SetLocalDepth(const int16_t localDepth) {
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

				glm::mat4 Transform::GetLocalTransform() const {
					return glm::translate(glm::mat4(1.0f), m_LocalPosition) *
							glm::rotate(glm::mat4(1.0f), glm::radians(m_LocalRotation), m_LocalRotationAxis) *
								glm::scale(glm::mat4(1.0f), m_LocalScale);
				}
			}
		}
	}
}
