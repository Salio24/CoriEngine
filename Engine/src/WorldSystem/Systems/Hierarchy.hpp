#pragma once
#include "System.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class Hierarchy final : public System {
			public:

				bool Create();

				static constexpr SystemPriority Priority = 50;
			protected:
				friend Entity;

				static std::expected<void, Core::CoriError<>> SetParent(Entity subject, Entity parent);

				static std::expected<void, Core::CoriError<>> LinkToParent(Entity subject, Entity parent);

				static void UnlinkFromParent(Entity subject);

				static std::expected<std::vector<Entity>, Core::CoriError<>> GetSiblings(Entity subject);

				static std::expected<std::vector<Entity>, Core::CoriError<>> GetChildren(Entity subject);

				static std::expected<Entity, Core::CoriError<>> GetParent(Entity subject);

				static std::expected<Entity, Core::CoriError<>> FindChildByName(Entity subject, const char* name);

				static std::expected<Entity, Core::CoriError<>> FindChildByName(Entity subject, const std::string_view name);

				static std::expected<Entity, Core::CoriError<>> FindChildByName(Entity subject, const std::string& name);

				static void DestroyChildren(Entity subject);

				static void PrintHierarchy(Entity subject);

				static void DrawHierarchyRecursive(const Entity& entity, const std::string& prefix, const bool isLast);

			private:
				void OnHierarchyComponentDestroyed(entt::registry& registry, entt::entity entity);
			};
		}
	}
}
