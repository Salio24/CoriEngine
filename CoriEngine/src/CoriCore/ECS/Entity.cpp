#include "Entity.hpp"
#include "Scene.hpp"

namespace Cori {
	//for internal use ONLY
	static Scene* m_ViewScene;

	Entity::Entity(const entt::entity& entity) {
		m_EntityHandle = entt::handle{ m_ViewScene->m_Registry, entity };
	}

	void Entity::SetViewScene(Scene* ptr) {
		m_ViewScene = ptr;
	}

}