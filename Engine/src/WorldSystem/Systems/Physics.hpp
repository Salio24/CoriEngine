#pragma once
#include "System.hpp"
#include "Physics/Physics.hpp"

#ifndef CORI_PHYSICS_TASK_POOL_SIZE
	#define CORI_PHYSICS_TASK_POOL_SIZE 64
#endif

namespace Cori {
	namespace World {
		namespace Systems {
			/**
			 * @brief System responsible for physics.
			 * @details This system is not registered by default, and thus is optional. Required by Trigger system for triggers to work.
			 */
			class PhysicsSystem final : public System {
			public:

				void OnTickUpdate(Core::GameTimer& gameTimer) override;

				bool Create(Physics::World::Params params);

				/**
				 * @brief Retries the reference to the Box2D world.
				 * @return Non const world reference.
				 */
				Physics::WorldRef GetWorld() { return m_World; }

				static constexpr SystemPriority Priority = 5;

				static void* EnqueueTask(b2TaskCallback* task, int32_t itemCount, int32_t minRange, void* taskContext, void* userContext);
				static void FinishTask(void* taskPtr, void* userContext);

			private:
				struct Box2DTaskGroup {
					std::vector<std::future<void>> futures;
				};

				Physics::World m_World;

				uint16_t m_WorkerCount{ 1 };
				std::array<Box2DTaskGroup, CORI_PHYSICS_TASK_POOL_SIZE> m_TaskPool;
				std::atomic<uint16_t> m_CurrentTaskIndex;

				void OnRigidBodyCreate(entt::registry& registry, entt::entity entity);
			};
		}
	}
}