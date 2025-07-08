#pragma once
#include "Core/Utility/TemplateUtils.hpp"
#include "Renderer/Primitives.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			class Render;
		}
	}

	namespace Graphics {
		template<typename Primitive> requires Utils::OneOf<Primitive, QuadPrimitive>
		class PrimitivePool {
		public:
			PrimitivePool(uint32_t initialCapacity, Cori::Scene* scene) : ParentScene(scene) {
				m_PrimitivePool.resize(initialCapacity);
				m_InvalidIndexes.reserve(initialCapacity);

				m_AvailableIndexes.resize(initialCapacity);
				std::iota(m_AvailableIndexes.rbegin(), m_AvailableIndexes.rend(), 0);
			}
			~PrimitivePool() {}

			uint32_t GetAvailableIndex() {
				if (m_AvailableIndexes.empty()) {
					uint32_t oldPoolSize = m_AvailableIndexes.size();
					m_PrimitivePool.resize(oldPoolSize * m_PoolGrowthFactor);
					m_AvailableIndexes.reserve(oldPoolSize * m_PoolGrowthFactor);
					m_InvalidIndexes.reserve(oldPoolSize * m_PoolGrowthFactor);

					uint32_t oldAvailableIndexesSize = m_AvailableIndexes.size();

					m_AvailableIndexes.resize(oldAvailableIndexesSize + oldPoolSize * (m_PoolGrowthFactor - 1));

					std::iota(m_AvailableIndexes.begin() + oldAvailableIndexesSize, m_AvailableIndexes.end(), oldPoolSize);

					return oldPoolSize;
				}

				uint32_t index = m_AvailableIndexes.back();
				m_AvailableIndexes.pop_back();
				return index;
			}

			void InvalidateIndex(uint32_t index) {
				if (index < m_PrimitivePool.size()) {
					m_PrimitivePool[index].SetValidity(false);
					m_InvalidIndexes.push_back(index);
					return;
				}
				CORI_CORE_WARN_TAGGED({"Primitive Pool"}, "Pool Type: {}, can invalidate index '{}', index out of bounds", typeid(Primitive).name(), index);
			}


			std::pair<Primitive*, uint32_t> AddPrimitive(Primitive primitive) {
				uint32_t index = GetAvailableIndex();
				m_PrimitivePool[index] = std::move(primitive);

				return {&m_PrimitivePool[index], index};
			}

			//Well, templates it is, then.
			Primitive* GetPrimitive(uint32_t index) {
				return &m_PrimitivePool[index];
			}

			void SortPoolByTexture();




			protected:

			friend class Cori::Scene;

			Cori::Scene* ParentScene;
		private:
			std::vector<Primitive> m_PrimitivePool;
			std::vector<uint32_t> m_AvailableIndexes;
			std::vector<uint32_t> m_InvalidIndexes;

			uint32_t m_InterstitialIndexesCount;

			static constexpr uint8_t m_PoolGrowthFactor = 3;
		};

	}
}
