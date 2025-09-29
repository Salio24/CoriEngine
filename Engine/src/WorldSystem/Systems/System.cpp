#include "System.hpp"
#include "WorldSystem/SceneManager.hpp"

namespace Cori {
	namespace World {
		void System::SetOwnerScene(const Scene* scene) {
			const auto result = SceneManager::GetScene(scene->GetName());
			if (result) {
				m_Owner = result.value();
			}
		}
	}
}