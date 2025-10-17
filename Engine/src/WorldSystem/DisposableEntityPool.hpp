#pragma once
#include "Entity.hpp"
#include "SceneHandle.hpp"
#include "Core/Error.hpp"

namespace Cori {
	namespace World {
		/**
		 * @brief Creates a pool with disposable entities.
		 * @details It is usefully when you need some entity to perform some task, but you don't want to keep track of it, so basically fire and forget style.
		 * \n Without this class you would need to:
		 * \n 1: Create the entity
		 * \n 2: Add components that you need to it
		 * \n 3: Do the work you need to do
		 * \n 4: Delete the entity
		 * \n This is far from optimal, DisposableEntityPool lets you create and define your entities once and then reuse them.
		 * @tparam PoolSize Desired size of the pool.
		 */
		template <uint16_t PoolSize>
		class DisposableEntityPool {
		public:
			DisposableEntityPool() {
				m_Mask.set();
				m_FreeIndexes.reserve(PoolSize);
			}

			/**
			 * @brief Initializes the pool.
			 * @param scene Scene that pool will live in.
			 * @param compositor Entity compositor, you need to define how to create the entity here, you're given scene handle and expected to return Entity, this functor will be used to create the entities.
			 * @param resetter Lets you define how to clean entities after they've been used, you're given the entity that is being freed, wll be run for every entity that you're freeing with FreeIndex.
			 */
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

			/**
			 * @brief Finds a free entity in a pool (if any) in O(1) time. After retrieving the entity will be considered busy and will no longer be retrievable until you call FreeIndex on this entity index.
			 * @return Expected object with a pair on Entity and its internal index (used to free the entity later) on success, on failure returns PossibleErrors object.
			 */
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

					return std::unexpected(Core::CoriError<uint16_t>("Ran out of free indexes, all entities are currently busy.", "Pool Capacity", PoolSize));
				}

				return std::unexpected(Core::CoriError("Disposable entity pool is not initialized, call Init first!"));
			}


			/**
			 * @brief Retries the entity at a particular index, regardless if it's free or not.
			 * @param index Index to retrieve an entity from.
			 * @return Expected object with entity on success, on failure returns PossibleErrors object.
			 */
			std::expected<Entity, Core::PossibleErrors<Core::CoriError<uint16_t>, Core::CoriError<>>> GetEntityAtIndex(const uint16_t index) {
				if (m_Initialized) {
					if (index < m_FreeIndexes.size()) {
						return m_Pool[index];
					}

					return std::unexpected(Core::CoriError<uint16_t>(std::format("Index '{}' out of bounds.", index), "Pool Capacity", PoolSize));
				}

				return std::unexpected(Core::CoriError("Disposable entity pool is not initialized, call Init first!"));
			}

			/**
			 * @brief Frees the entity at the given index, a resetter will be run for the entity at the specified index, the entity will be considered free after calling this.
			 * @param slot Slot or index to free.
			 */
			void FreeIndex(const uint16_t slot) {
				if (!m_Mask[slot] && m_Initialized) {
					m_FreeIndexes.push_back(slot);
					m_Mask[slot] = true;
					Entity entity = m_Pool[slot];
					m_Resetter(entity);
					entity.SetActive(false);
				}
			}

			/**
			 * @brief Returns the total capacity of the pool, how many entities can it fit in total.
			 * @return Pool capacity.
			 */
			[[nodiscard]] uint16_t Capacity() const {
				return PoolSize;
			}

			/**
			 * @brief Returns the amount of free entities currently present in the pool.
			 * @return Pool size.
			 */
			[[nodiscard]] uint16_t Size() const {
				return m_FreeIndexes.size();
			}

			/**
			 * @brief Frees all indexes at once.
			 */
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
