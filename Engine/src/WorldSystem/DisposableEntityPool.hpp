#pragma once
#include "Entity.hpp"
#include "SceneHandle.hpp"
#include "Core/Error.hpp"

namespace Cori {
	namespace World {
		template <uint16_t PoolSize>
		class DisposableEntityPool {
		public:
			explicit DisposableEntityPool(SceneHandle scene, std::function<void(Entity&)>&& compositor) {
				for (uint16_t i = 0; i < PoolSize; ++i) {
					Entity entity = scene.CreateBlankEntity();
					compositor(entity);
					m_Pool[i] = entity;
				}
			}

			std::expected<std::pair<Entity, uint16_t>, Core::CoriError<>> GetFreeEntity() {

			}

			std::expected<Entity, Core::CoriError<>> GetEntityAtIndex(uint16_t index) {

			}

			void SetSlotActivityState(const uint16_t slot, const bool state) {
				if (state) {
					// activate slot

				} else {
					// deactivate slot, entity is busy
				}

			}

		private:
			std::array<Entity, PoolSize> m_Pool;
		};
	}
}
