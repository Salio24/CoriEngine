#pragma once
#include "WorldSystem/SceneHandle.hpp"
#include "Concept.hpp"

//* \n SystemPriority can be used to ensure one system is updated before another, it is safe to have more than one system with the same priority,
//* but I highly discourage toy from doing that as in that case the order is not enforced and can vary from run to run.

namespace Cori {
	namespace World {
		/**
		 * @brief System part of ECS.
		 * @details Systems are a crucial part of the world and scene, I'm not going to describe what systems are in an ECS scope, you can google that.
		 * \n In Cori, you can register, unregister systems for the scene, a scene can only have one instance of a particular system.
		 * \n Systems can have a specific priority to them, that way you can control what systems are updated first.
		 * \n To define a system you need to:
		 * \n 1: Derive from this class.
		 * \n 2: Create a method 'Create' that can take any amount of arguments and returns a bool.
		 * If 'Create' returns false the system will not be registered for the scene.
		 * \n 3: A static public member of type 'SystemPriority' named 'Priority'
		 * @warning You should put all your initialization code into the 'Create' method, because m_Owner (SceneHandle) is only valid after the constructor has run.
		 */
		class System {
		public:
			virtual ~System() = default;

			System(const System&) = delete;
			System& operator=(const System&) = delete;
			System(System&&) = delete;
			System& operator=(System&&) = delete;

			virtual void OnUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {}

			virtual void OnTickUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {}

			virtual void OnImGuiRender([[maybe_unused]] Core::GameTimer& gameTimer) {}

		protected:
			SceneHandle m_Owner{ nullptr };
			System() = default;
		private:
			friend Scene;
			void SetOwnerScene(const Scene* scene);
		};
	}
}
