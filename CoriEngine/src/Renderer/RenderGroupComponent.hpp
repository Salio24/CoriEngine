#pragma once
#include "SceneSystem/Scene.hpp"
#include "SceneSystem/Entity.hpp"
#include "Renderer/PrimitivePool.hpp"
#include "Core/Utility/StringHash.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			struct RenderGroup {
				Cori::Scene* m_ParentScene{nullptr};
				Cori::Entity m_ParentEntity;

				RenderGroup() = default;
				RenderGroup(Cori::Scene* scene, Cori::Entity entity) : m_ParentScene(scene), m_ParentEntity(entity) {}


				template <typename T> requires Utils::OneOf<T, Graphics::QuadPrimitive>
				std::expected<T*, const char*> AddPrimitive(const typename T::Descriptor& descriptor, const Utils::StringHash id) {
					if (m_NamedPrimitives.contains(id)) { return std::unexpected("Error: Primitive the specified name already exists in the hash map"); }

					T primitive;
					primitive.worldPosition = m_WorldPosition;
					primitive.localPosition = descriptor.localPosition;
					primitive.size = descriptor.size;
					primitive.tintColor = descriptor.tintColor;
					primitive.owner = m_ParentEntity;
					primitive.uvs = descriptor.uvs;
					primitive.rotation = descriptor.rotation;
					primitive.layer = descriptor.layer;
					primitive.SetTexture(descriptor.texture);

					auto [primPtr, index] = m_ParentScene->GetPoolForType<T>().AddPrimitive(primitive);

					m_NamedPrimitives.insert({id, {index, Graphics::PrimitiveTypeTraits<T>::type}});

					return primPtr;
				}

				template <typename T> requires Utils::OneOf<T, Graphics::QuadPrimitive>
				std::expected<T*, const char*> GetPrimitive(const Utils::StringHash id) {
					if (m_NamedPrimitives.contains(id)) {
						auto [index, type] = m_NamedPrimitives.at(id);
						if (Graphics::PrimitiveTypeTraits<T>::IsValid(type) != type) {
							return std::unexpected("Error: Specified primitive type doesnt match the type at the specified id.");
						}

						return m_ParentScene->GetPoolForType<T>().GetPrimitive(index);
					}
					return std::unexpected("Error: No primitive with the specified id found in the hashmap.");
				}

				void SetWorldPosition(glm::vec2 position) {
					if (!m_NamedPrimitives.empty()) {
						auto& pool = m_ParentScene->GetPoolForType<Graphics::QuadPrimitive>();
						for (auto& pos : m_NamedPrimitives) {
							pool.GetPrimitive(pos.second.index)->worldPosition = position;
						}
					}
					m_WorldPosition = position;
				}


				glm::vec2 GetWorldPosition() const {
					return m_WorldPosition;
				}

			private:

				std::unordered_map<Utils::StringHash, Graphics::PrimitiveHandle> m_NamedPrimitives;

				glm::vec2 m_WorldPosition{ 0.0f, 0.0f };
			};

		}
	}
}