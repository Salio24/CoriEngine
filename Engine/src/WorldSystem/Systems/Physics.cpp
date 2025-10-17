#include "Physics.hpp"
#include "Physics/Physics.hpp"
#include "Core/Application.hpp"
#include "WorldSystem/Components.hpp"

#ifndef CORI_MAX_PHYSICS_THREADS
	#define CORI_MAX_PHYSICS_THREADS 4
#endif

namespace Cori {
	namespace World {
		namespace Systems {
			void PhysicsSystem::OnTickUpdate(Core::GameTimer& gameTimer) {
				CORI_PROFILE_FUNCTION();
				if (CORI_MULTITHREADED_PHYSICS) {
					m_CurrentTaskIndex.store(0, std::memory_order_relaxed);
				}
				m_World.Step(gameTimer.GetTimestep(), 4);
				//m_Owner.GetContextComponent<Components::Scene::PhysicsWorld>().Step(gameTimer.GetTimestep(), 4);
			}

			bool PhysicsSystem::Create(Cori::Physics::World::Params params) {
				if (CORI_MULTITHREADED_PHYSICS) {
					if (Core::Application::GetWorkerCount() == 1) {
						m_WorkerCount = 1;
					} else if (Core::Application::GetWorkerCount() > CORI_MAX_PHYSICS_THREADS) {
						m_WorkerCount = CORI_MAX_PHYSICS_THREADS;
					} else {
						m_WorkerCount = Core::Application::GetWorkerCount() - 1;
					}

					params.workerCount = m_WorkerCount;
					params.enqueueTask = EnqueueTask;
					params.finishTask = FinishTask;
					params.userTaskContext = this;
					//m_Owner.AddContextComponent<Components::Scene::PhysicsWorld>(params);
				}

				m_World = Cori::Physics::World(params);

				m_Owner.GetRegistry().on_construct<Components::Entity::RigidBody>().connect<&PhysicsSystem::OnRigidBodyCreate>(this);
				return true;
			}

			void* PhysicsSystem::EnqueueTask(b2TaskCallback* task, int32_t itemCount, int32_t minRange, void* taskContext, void* userContext) {
				auto* physicsSystem = static_cast<PhysicsSystem*>(userContext);
				const uint16_t workerCount = physicsSystem->m_WorkerCount;

				if (workerCount <= 1 || itemCount < minRange) {
					task(0, itemCount, 0, taskContext);
					return nullptr;
				}

				const uint16_t taskIndex = physicsSystem->m_CurrentTaskIndex.fetch_add(1, std::memory_order_relaxed);
				CORI_CORE_ASSERT(taskIndex < CORI_PHYSICS_TASK_POOL_SIZE, "Exceeded the pre-allocated Box2D task pool size!");

				Box2DTaskGroup& taskGroup = physicsSystem->m_TaskPool[taskIndex];

				taskGroup.futures.clear();

				int32_t itemsPerThread = (itemCount + workerCount - 1) / workerCount;
				itemsPerThread = std::max(itemsPerThread, minRange);

				int32_t startIndex = 0;
				for (uint16_t i = 0; i < workerCount && startIndex < itemCount; ++i) {
					const int32_t endIndex = std::min(startIndex + itemsPerThread, itemCount);

					taskGroup.futures.push_back(Core::Application::SubmitWorkerTask(
						[=]() {
							task(startIndex, endIndex, i, taskContext);
						}
					));
					startIndex = endIndex;
				}

				return &taskGroup;
			}

			void PhysicsSystem::FinishTask(void* taskPtr, void* userContext) {
				if (taskPtr == nullptr) {
					return;
				}

				auto* taskGroup = static_cast<Box2DTaskGroup*>(taskPtr);

				for (auto& future : taskGroup->futures) {
					future.get();
				}
			}

			void PhysicsSystem::OnRigidBodyCreate(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& rb = e.GetComponents<Components::Entity::RigidBody>();
				const auto type = rb.GetType();
				if (type == b2_kinematicBody || type == b2_dynamicBody) {
					auto& ud = e.AddComponent<Cori::Physics::BodyUserData>();
					rb.SetUserData(&ud);
				}
			}
		}
	}
}