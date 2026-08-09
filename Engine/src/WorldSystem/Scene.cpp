#include "Scene.hpp"
#include "Graphics/CameraController.hpp"
#include "Physics/Triggers/Trigger.hpp"
#include "Graphics/Animator/QuadAnimator.hpp"
#include "Systems/RenderSync.hpp"

namespace Cori {
	namespace World {
		std::shared_ptr<Scene> Scene::Create(std::string name) {
			return std::shared_ptr<Scene>(new Scene(std::move(name)));
		}

		Scene::Scene(std::string name) : m_Name(std::move(name)) {
			AddContextComponent<Components::Scene::Camera>();
			m_ActiveCamera.BindCameraComponent(&GetContextComponent<Components::Scene::Camera>());
			CORI_CORE_INFO_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Scene::Self }, "Scene: '{}' created.", m_Name);
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
		}

		void Scene::OnTickUpdate(Core::GameTimer& gameTimer) {
			for (auto type : m_SystemPriority | std::views::values) {
				m_RegisteredSystems[type]->OnTickUpdate(gameTimer);
			}
		}

		void Scene::OnImGuiRender(Core::GameTimer& gameTimer) {
			for (auto type : m_SystemPriority | std::views::values) {
				m_RegisteredSystems[type]->OnImGuiRender(gameTimer);
			}
		}

		bool Scene::SubmitForRender() {
			auto result = GetSystem<Systems::RenderSync>();
			if (result) {
				return result.value().lock()->SubmitForRendering();
			}

			result.error().Ignore();
			return true;
		}

		bool Scene::WaitForFrameData() {
			auto result = GetSystem<Systems::RenderSync>();
			if (result) {
				return result.value().lock()->WaitForFrameData();
			}

			result.error().Ignore();
			return true;
		}

		bool Scene::PrepareFrameData() {
			auto result = GetSystem<Systems::RenderSync>();
			if (result) {
				return result.value().lock()->PrepareFrameData();
			}

			result.error().Ignore();
			return true;
		}
	}
}
