#pragma once
#include "Renderer/Texture.hpp"
#include "SceneSystem/Entity.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			class Render;
		}
	}

	namespace Graphics {
		struct QuadPrimitive {
			glm::vec2 worldPosition; // add , SetWorldPos method to render component, and update this in it
			glm::vec2 localPosition;
			glm::vec2 size;
			glm::vec4 tintColor;
			UVs uvs;
			Entity owner;
			float rotation;
			uint8_t layer;
			bool hasSemiTransparency;

			void SetTexture(const std::shared_ptr<Texture2D>& t) {
				hasSemiTransparency = t->HasSemiTransparency();
				texture = t;
			}
		private:
			std::shared_ptr<Texture2D> texture;

		};


		using PrimitiveID = uint32_t;
		consteval PrimitiveID operator""_pid(const char* str, size_t) {
			return entt::hashed_string(str).value();
		}

		struct Primitive {


		};

		class PrimitivePool {
		public:
			~PrimitivePool();
			PrimitivePool();

			uint32_t GetAvailableIndex();

			void FreeIndex(uint32_t index);

			std::pair<QuadPrimitive&, uint32_t> AddPrimitive(const QuadPrimitive& quad, PrimitiveID id, const Components::Entity::Render* ownerComponent, const Entity& ownerEntity);

			void RemovePrimitive(const Components::Entity::Render* ownerComponent, PrimitiveID id);

			// this is wrong, i need some general class "Primitive" that can describe several types primitives. so use union
			// or stop fooling around and use templates like any sane person (in c++? lol)
			QuadPrimitive& GetPrimitive(const Components::Entity::Render* ownerComponent, PrimitiveID id);

			void SortPoolByTexture();









			protected:

			friend class Cori::Scene;

			Cori::Scene* ParentScene;
		private:
			std::vector<Primitive> m_PrimitivePool;
			std::vector<uint32_t> m_AvailableIndexes;

			uint32_t m_InterstitialIndexesCount;
		};

	}
}



