#include "Scene.hpp"
#include "Graphics/CameraController.hpp"
#include "Physics/Triggers/Trigger.hpp"
#include "Graphics/Renderer2D.hpp"
#include "Graphics/Animator/QuadAnimator.hpp"

namespace Cori {
	namespace World {
		std::shared_ptr<Scene> Scene::Create(std::string name) {
			return std::shared_ptr<Scene>(new Scene(std::move(name)));
		}

		Scene::Scene(std::string name) : m_Name(std::move(name)) {
			AddContextComponent<Components::Scene::Camera>();
			m_ActiveCamera.BindCameraComponent(&GetContextComponent<Components::Scene::Camera>());
			CORI_CORE_INFO_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Scene: '{}' created.", m_Name);

			[[maybe_unused]] auto nameAndTagGroup = m_Registry.group<Components::Entity::Name, Components::Entity::Tag>();
		}

		Scene::~Scene() {
			m_Registry.clear();
			CORI_CORE_INFO_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Scene: '{}' destroyed.", m_Name);
		}

		Entity Scene::CreateBlankEntity() {
			entt::entity entity = m_Registry.create();
			Entity e = entt::handle{m_Registry, entity};
			e.AddComponent<Internal::SceneID>(m_SceneID);
			return e;
		}

		Entity Scene::CreateEntity(const std::string& name, const Utility::HashedTag64& tag) {
			entt::entity entity = m_Registry.create();
			auto& nameComp = m_Registry.emplace<Components::Entity::Name>(entity);
			nameComp.m_Name = name;
			m_Registry.emplace<Components::Entity::Tag>(entity, tag);
			m_Registry.emplace<Components::Entity::Hierarchy>(entity);
			const auto& uuidComp = m_Registry.emplace<Components::Entity::UUID>(entity);
			m_UUIDToEntity.insert({ uuidComp.m_UUID, entity });
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Created Entity With ID: {}, Version: {}, Name: {}, Tag: {}", entt::to_integral(entity), entt::to_version(entity), name, tag.GetDebugName());
			Entity e = entt::handle{m_Registry, entity};
			e.AddComponent<Components::Entity::Transform>();
			e.AddComponent<Internal::SceneID>(m_SceneID);
			return e;
		}

		std::expected<void, Core::CoriError<>> Scene::AddEntityToCache(const Entity entity, const Utility::StringHash32 key) {
			if (m_EntityCache.contains(key)) {
				return std::unexpected(Core::CoriError("Entry with the given key already exists, make sure you're not reusing the key. It's also possible (but very unlikely), that you got a hash collision, try to change the key a bit."));
			}
			m_EntityCache.insert({ key, entity.GetRawEntity() });
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Entity '{}' added to scene local cache, key: '{}'", entity.GetDebugData(), key);
			return{};
		}

		std::expected<Entity, Core::CoriError<>> Scene::GetEntityFromCache(const Utility::StringHash32 key) {
			if (!m_EntityCache.contains(key)) {
				return std::unexpected(Core::CoriError("No entity with the specified key found in scene local cache."));
			}
			if (!m_Registry.valid(m_EntityCache.at(key))) {
				m_EntityCache.erase(key);
				return std::unexpected(Core::CoriError("Entity at the specified key is invalid, removing this entry."));
			}
			return Entity{ { m_Registry, m_EntityCache.at(key) } };
		}

		void Scene::RemoveEntityFromCache(const Utility::StringHash32 key) {
			if (m_EntityCache.contains(key)) {
				const Entity entity = entt::handle{ m_Registry, m_EntityCache.at(key) };
				m_EntityCache.erase(key);
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Entity '{}' removed from scene local cache, key: '{}'", entity.GetDebugData(), key);
			}
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Trying to remove an Entity from scene local cache, provided key: '{}', but cache has no such entry.", key);
		}

		std::expected<Entity, Core::CoriError<>> Scene::FindEntity(const std::string& name) {
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Performing slow scene-wide search for entity named: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", name);
			const auto view = m_Registry.view<Components::Entity::Name>();
			for (auto entity : view) {
				if (name == view.get<Components::Entity::Name>(entity).m_Name) {
					return Entity{ { m_Registry, entity } };
				}
			}
			return std::unexpected(Core::CoriError("No entity found with the specified name."));
		}
		std::expected<Entity, Core::CoriError<>> Scene::FindEntity(const std::string& name, const Utility::HashedTag64& tag) {
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Performing slow scene-wide search for entity with tag: '{}', named: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", tag.GetDebugName(), name);
			const auto group = m_Registry.group<Components::Entity::Name, Components::Entity::Tag>();
			for (auto entity : group) {
				auto [nameComp , tagComp] = group.get<Components::Entity::Name, Components::Entity::Tag>(entity);
				if (name == nameComp.m_Name && tag == tagComp.m_Tag) {
					return Entity{ { m_Registry, entity } };
				}
			}
			return std::unexpected(Core::CoriError("No entity found with the specified name and tag."));
		}

		std::vector<Entity> Scene::GetEntitiesWithTag(const Utility::HashedTag64& tag) {
			std::vector<Entity> entities;
			CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Performing slow scene-wide collection of all entities with tag: '{}'. Consider caching it. This shouldn't be called every frame! Be aware.", tag.GetDebugName());
			const auto view = m_Registry.view<Components::Entity::Tag>();
			for (const auto entity : view) {
				auto& tagComp = view.get<Components::Entity::Tag>(entity);
				if (tag == tagComp.m_Tag) {
					entities.emplace_back(entt::handle{ m_Registry, entity });
				}
			}
			return entities;
		}

		void Scene::DestroyEntity(Entity entity) {
			if (!entity.IsValid()) { return; }
			if (entity.HasComponents<Components::Entity::Hierarchy>()) {
				entity.UnlinkFromParent();
			}
			const auto& uuidComp = entity.GetComponents<Components::Entity::UUID>();
			m_UUIDToEntity.erase(uuidComp.m_UUID);
			m_Registry.destroy(entity.GetRawEntity());
		}

		void Scene::OnUpdate([[maybe_unused]] Core::GameTimer& gameTimer) {
			CORI_PROFILE_FUNCTION();

			for (auto type : m_SystemPriority | std::views::values) {
				m_RegisteredSystems[type]->OnUpdate(gameTimer);
			}

			Graphics::Renderer2D::SubmitScene(this);
			Graphics::Renderer2D::EndFrame(GetContextComponent<Components::Scene::Camera>());
		}

		void Scene::OnTickUpdate(Core::GameTimer& gameTimer) {
			if (HasContextComponent<Components::Scene::PhysicsWorld>()) {
				GetContextComponent<Components::Scene::PhysicsWorld>().Step(gameTimer.GetTimestep(), 4);
			}

			for (auto type : m_SystemPriority | std::views::values) {
				m_RegisteredSystems[type]->OnTickUpdate(gameTimer);
			}
		}

		void Scene::OnImGuiRender(Core::GameTimer& gameTimer) {
			for (auto type : m_SystemPriority | std::views::values) {
				m_RegisteredSystems[type]->OnImGuiRender(gameTimer);
			}
		}

		bool Scene::OnBind() {
			return true;
		}

		bool Scene::OnUnbind() {
			return true;
		}
	}
}
