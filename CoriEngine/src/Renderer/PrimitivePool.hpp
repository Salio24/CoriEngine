#pragma once
#include "Core/Utility/TemplateUtils.hpp"
#include "Renderer/Primitives.hpp"

#ifndef CORI_GRAPHICS_QUAD_POOL_INITIAL_SIZE
	#define CORI_GRAPHICS_QUAD_POOL_INITIAL_SIZE 65536
#endif

namespace Cori {
	namespace Graphics {
		template<typename Primitive> requires Utils::OneOf<Primitive, QuadPrimitive>
		class PrimitivePool {
		public:
			PrimitivePool(uint32_t initialCapacity, Cori::Scene* scene) : ParentScene(scene) {
				m_PrimitivePool.reserve(initialCapacity);
				m_BoolMask.resize(initialCapacity, false);

				//m_InvalidIndexes.reserve(initialCapacity);
				//m_AvailableIndexes.resize(initialCapacity);
				//std::iota(m_AvailableIndexes.rbegin(), m_AvailableIndexes.rend(), 0);
			}
			~PrimitivePool() {}

			uint32_t GetAvailableIndex() {
				//if (m_AvailableIndexes.empty()) {
				//	uint32_t oldPoolSize = m_AvailableIndexes.size();
				//	m_PrimitivePool.resize(oldPoolSize * m_PoolGrowthFactor);
				//	m_AvailableIndexes.reserve(oldPoolSize * m_PoolGrowthFactor);
				//	m_InvalidIndexes.reserve(oldPoolSize * m_PoolGrowthFactor);
//
				//	uint32_t oldAvailableIndexesSize = m_AvailableIndexes.size();
//
				//	m_AvailableIndexes.resize(oldAvailableIndexesSize + oldPoolSize * (m_PoolGrowthFactor - 1));
//
				//	std::iota(m_AvailableIndexes.begin() + oldAvailableIndexesSize, m_AvailableIndexes.end(), oldPoolSize);
//
				//	return oldPoolSize;
				//}
//
				////uint32_t index = m_AvailableIndexes.back();
				////m_AvailableIndexes.pop_back();
				//return index;
			}

			void GrowPool() {
				uint32_t oldPoolSize = m_PrimitivePool.size();
				m_PrimitivePool.reserve(oldPoolSize * m_PoolGrowthFactor);
				m_BoolMask.resize(oldPoolSize * m_PoolGrowthFactor, false);
			}

			// rename to invalidatePrimitive
			void InvalidatePrimitive(uint32_t index) {
				if (index < m_PrimitivePool.size()) {
					m_PrimitivePool[index].SetValidity(false);
					m_BoolMask[index] = true;
					m_InvalidPrimitiveCount++;
					//m_InvalidIndexes.push_back(index);
					return;
				}
				CORI_CORE_WARN_TAGGED({"Primitive Pool"}, "Pool Type: {}, can invalidate index '{}', index out of bounds", typeid(Primitive).name(), index);
			}


			std::pair<Primitive*, uint32_t> AddPrimitive(Primitive primitive) {
				//uint32_t index = GetAvailableIndex();
				//m_PrimitivePool[index] = std::move(primitive);
				if (m_PrimitivePool.capacity() == m_PrimitivePool.size()) {
					GrowPool();
				}
				m_PrimitivePool.emplace_back(primitive);
				uint32_t index = m_PrimitivePool.size() - 1;

				return {&m_PrimitivePool[index], index};
			}

			//Well, templates it is, then.
			Primitive* GetPrimitive(uint32_t index) {
				return &m_PrimitivePool[index];
			}

			void Defragment() {
				CORI_PROFILE_FUNCTION();

				if (!m_InvalidPrimitiveCount) {
					return;
				}

				uint32_t writeIndex = 0;
				for (uint32_t readIndex = 0; readIndex < m_PrimitivePool.size(); ++readIndex) {
					if (!m_BoolMask[readIndex]) {
						if (writeIndex != readIndex) {
							m_PrimitivePool[writeIndex] = std::move(m_PrimitivePool[readIndex]);
						}
						writeIndex++;
					} else {
						m_BoolMask[readIndex] = false;
					}
				}
				m_InvalidPrimitiveCount = 0;
				m_PrimitivePool.resize(writeIndex);
			}

			// texture sort
			void SortByTexture() {
				CORI_PROFILE_FUNCTION();

				ska_sort(
					begin(),
					end(),
					[](const QuadPrimitive& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.texture.get());
					}
				);
				// update indexes in rendergroup map
				// finish this
			}

			typename std::vector<Primitive>::iterator begin() { return m_PrimitivePool.begin(); }
			typename std::vector<Primitive>::iterator end() { return m_PrimitivePool.end(); }

			protected:

			friend class Cori::Scene;

			Cori::Scene* ParentScene;
		private:
			std::vector<Primitive> m_PrimitivePool;
			//std::vector<uint32_t> m_AvailableIndexes;
			//std::vector<uint32_t> m_InvalidIndexes;
			std::vector<bool> m_BoolMask;

			uint32_t m_InvalidPrimitiveCount{ 0 };

			static constexpr uint8_t m_PoolGrowthFactor = 3;
		};

	}
}
