#pragma once
#include "Core/Utility/TemplateUtils.hpp"
#include "Renderer/Primitives.hpp"
#include "SceneSystem/Scene.hpp"
#include <ska_sort.hpp>

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
			}
			~PrimitivePool() = default;

			// O(N) N - m_PrimitivePool.size
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
							Primitive& primitive = m_PrimitivePool[writeIndex];
							primitive.owner.template GetComponents<Cori::Components::Entity::RenderGroup>().UpdatePrimitiveIndex(primitive.id, writeIndex);
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
						return reinterpret_cast<uint64_t>(quad.m_Texture.get());
					}
				);

				for (uint32_t index = 0; index < m_PrimitivePool.size(); index++) {
					Primitive& primitive = m_PrimitivePool[index];
					primitive.owner.template GetComponents<Cori::Components::Entity::RenderGroup>().UpdatePrimitiveIndex(primitive.id, index);
				}
			}

			typename std::vector<Primitive>::iterator begin() { return m_PrimitivePool.begin(); }
			typename std::vector<Primitive>::iterator end() { return m_PrimitivePool.end(); }

		protected:
			std::pair<Primitive*, uint32_t> AddPrimitive(Primitive primitive) {
				if (m_PrimitivePool.capacity() == m_PrimitivePool.size()) {
					GrowPool();
				}
				m_PrimitivePool.emplace_back(primitive);
				uint32_t index = m_PrimitivePool.size() - 1;

				return {&m_PrimitivePool[index], index};
			}

			Primitive* GetPrimitive(uint32_t index) {
				return &m_PrimitivePool[index];
			}

			void InvalidatePrimitive(uint32_t index) {
				if (index < m_PrimitivePool.size()) {
					m_PrimitivePool[index].SetValidity(false);
					m_BoolMask[index] = true;
					m_InvalidPrimitiveCount++;
					return;
				}
				CORI_CORE_WARN_TAGGED({"Primitive Pool"}, "Pool Type: {}, can invalidate index '{}', index out of bounds", typeid(Primitive).name(), index);
			}

			friend class Components::Entity::RenderGroup;
			friend class Cori::Scene;

			Cori::Scene* ParentScene;
		private:

			void GrowPool() {
				uint32_t oldPoolSize = m_PrimitivePool.size();
				m_PrimitivePool.reserve(oldPoolSize * m_PoolGrowthFactor);
				m_BoolMask.resize(oldPoolSize * m_PoolGrowthFactor, false);
			}

			std::vector<Primitive> m_PrimitivePool;
			std::vector<bool> m_BoolMask;

			uint32_t m_InvalidPrimitiveCount{ 0 };

			static constexpr uint8_t m_PoolGrowthFactor = 3;
		};

	}
}
