#pragma once
#include "Entity.hpp"
#include "SceneHandle.hpp"
#include "Core/Error.hpp"

namespace Cori {
	namespace World {
		template <uint16_t PoolSize>
		class DisposableEntityPool {
		public:
			DisposableEntityPool() {
				m_Mask.set();
				m_FreeIndexes.reserve(PoolSize);
			}

			void Init(SceneHandle scene, std::function<Entity(SceneHandle&)>&& compositor, std::function<void(Entity&)>&& resetter) {
				for (uint16_t i = 0; i < PoolSize; ++i) {
					Entity entity = compositor(scene);
					entity.SetActive(false);
					m_Pool[i] = entity;
					m_FreeIndexes.push_back(i);
				}

				m_Resetter = resetter;
				m_Initialized = true;
			}

			std::expected<std::pair<Entity, uint16_t>, Core::PossibleErrors<Core::CoriError<uint16_t>, Core::CoriError<>>> GetFreeEntity() {
				if (m_Initialized) {
					if (m_Mask.count() > 0) {
						uint16_t freeIndex = m_FreeIndexes.back();
						m_FreeIndexes.pop_back();
						m_Mask[freeIndex] = false;
						Entity entity = m_Pool[freeIndex];
						entity.SetActive(true);
						return  std::make_pair(entity, freeIndex);
					}

					return std::unexpected(Core::CoriError<uint16_t>("Ran out of free indexes, all entities currently busy.", "Pool Capacity", PoolSize));
				}

				return std::unexpected(Core::CoriError("Disposable entity pool is not initialized, call Init first!"));
			}

			std::expected<Entity, Core::PossibleErrors<Core::CoriError<uint16_t>, Core::CoriError<>>> GetEntityAtIndex(const uint16_t index) {
				if (m_Initialized) {
					if (index < m_FreeIndexes.size()) {
						return m_Pool[index];
					}

					return std::unexpected(Core::CoriError<uint16_t>(std::format("Index '{}' out of bounds.", index), "Pool Capacity", PoolSize));
				}

				return std::unexpected(Core::CoriError("Disposable entity pool is not initialized, call Init first!"));
			}

			void FreeIndex(const uint16_t slot) {
				if (!m_Mask[slot] && m_Initialized) {
					m_FreeIndexes.push_back(slot);
					m_Mask[slot] = true;
					Entity entity = m_Pool[slot];
					m_Resetter(entity);
					entity.SetActive(false);
				}
			}

			uint16_t Capacity() const {
				return PoolSize;
			}

			uint16_t Size() const {
				return m_FreeIndexes.size();
			}

			void FreeAll() {
				for (uint16_t i = 0; i < PoolSize; ++i) {
					FreeIndex(i);
				}
			}

		private:
			std::vector<uint16_t> m_FreeIndexes;
			std::array<Entity, PoolSize> m_Pool;
			std::function<void(Entity&)> m_Resetter;
			std::bitset<PoolSize> m_Mask;
			bool m_Initialized{ false };
		};
	}
}
