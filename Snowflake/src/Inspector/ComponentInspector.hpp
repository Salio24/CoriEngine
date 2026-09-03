#pragma once
#include <Cori.hpp>
#include "AssetDragDropPayload.hpp"

namespace Snowflake {
	class ComponentInspector {
	public:
		static constexpr const char* s_DefaultName{ "Inspector" };

		void Draw(bool* open, Cori::World::SceneHandle scene, entt::entity selected, const char* name = s_DefaultName);

		void InvalidateRotationCache();

	private:
		void DrawEntityHeader(Cori::World::Entity entity);

		void DrawTransform(Cori::World::Entity entity);

		void DrawRendering(Cori::World::Entity entity);

		entt::entity m_CachedRotationEntity{ entt::null };
		glm::vec3 m_CachedRotation{ 0.0f };
		bool m_RotationCacheValid{ false };

		char m_NameBuffer[128]{};
		entt::entity m_NameBufferEntity{ entt::null };
	};
}
