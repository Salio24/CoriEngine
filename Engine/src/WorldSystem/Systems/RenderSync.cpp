#include "RenderSync.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			bool RenderSync::PrepareFrameData() {
				if (m_Pending) {
					return true;
				}

				auto* renderer = Graphics::MasterRenderer::Get().Resolve(m_RendererHandle);
				if (!renderer) {
					return true;
				}

				Graphics::FrameData* fd = renderer->PopRecycledFrameData();
				if (!fd) {
					return false;
				}

				fd->Clear();

				auto view1 = m_Owner.StaticView<Components::Entity::Rendering, Components::Entity::Transform, Components::Entity::Internal::TransformDirtyForRendererFlag, Components::Entity::Internal::RenderComponentDirtyFlag>();

				auto view2 = m_Owner.StaticView<Components::Entity::Rendering, Components::Entity::Transform, Components::Entity::Internal::RenderComponentDirtyFlag>(Exclude<Components::Entity::Internal::TransformDirtyForRendererFlag>());

				auto view3 = m_Owner.StaticView<Components::Entity::Rendering, Components::Entity::Transform, Components::Entity::Internal::TransformDirtyForRendererFlag>(Exclude<Components::Entity::Internal::RenderComponentDirtyFlag>());

				for (auto e : view1) {
					auto& rc = view1.Get<Components::Entity::Rendering>(e);
					auto& tc = view1.Get<Components::Entity::Transform>(e);

					if (rc.m_NewRegister) {
						rc.m_RenderObjectHandle = renderer->AllocateRenderObjectHandle();
						rc.m_NewRegister = true;
					}

					Graphics::Patch patch;
					if (rc.m_MaterialDirty) {
						patch.material = rc.GetMaterial();
						rc.m_MaterialDirty = false;
					}

					if (rc.m_MeshDirty) {
						patch.mesh = rc.GetMesh();
						rc.m_MeshDirty = false;
					}

					if (rc.m_UvOffsetsDirty) {
						patch.uvOffsets = rc.GetUVOffsets();
						patch.isNewUvOffsets = true;
						rc.m_UvOffsetsDirty = false;
					}

					patch.transform = tc.GetLocalTransform();
					patch.isNewTransform = true;

					if (rc.m_NewRegister) {
						patch.isRegisterRequest = true;
						rc.m_NewRegister = false;
					}

					patch.handle = rc.m_RenderObjectHandle;
					fd->patches.emplace_back(patch);
				}

				for (auto e : view2) {
					auto& rc = view2.Get<Components::Entity::Rendering>(e);

					Graphics::Patch patch;

					if (rc.m_NewRegister) {
						rc.m_RenderObjectHandle = renderer->AllocateRenderObjectHandle();
						rc.m_NewRegister = true;
						auto& tc = view2.Get<Components::Entity::Transform>(e);
						patch.transform = tc.GetLocalTransform();
						patch.isNewTransform = true;
					}

					if (rc.m_MaterialDirty) {
						patch.material = rc.GetMaterial();
						rc.m_MaterialDirty = false;
					}

					if (rc.m_MeshDirty) {
						patch.mesh = rc.GetMesh();
						rc.m_MeshDirty = false;
					}

					if (rc.m_UvOffsetsDirty) {
						patch.uvOffsets = rc.GetUVOffsets();
						patch.isNewUvOffsets = true;
						rc.m_UvOffsetsDirty = false;
					}

					if (rc.m_NewRegister) {
						patch.isRegisterRequest = true;
						rc.m_NewRegister = false;
					}

					patch.handle = rc.m_RenderObjectHandle;
					fd->patches.emplace_back(patch);
				}

				for (auto e : view3) {
					auto& rc = view3.Get<Components::Entity::Rendering>(e);
					auto& tc = view3.Get<Components::Entity::Transform>(e);

					Graphics::Patch patch;

					patch.transform = tc.GetLocalTransform();
					patch.isNewTransform = true;

					patch.handle = rc.m_RenderObjectHandle;
					fd->patches.emplace_back(patch);
				}

				m_Owner.Clear<Components::Entity::Internal::RenderComponentDirtyFlag, Components::Entity::Internal::TransformDirtyForRendererFlag>();

				m_FrameSubmitted = false;
				m_Pending = fd;
				return true;
			}

			bool RenderSync::Create(Graphics::SceneRenderer::CreateInfo&& createInfo) {
				m_Owner.GetRegistry().on_construct<Components::Entity::Rendering>().connect<&RenderSync::OnRenderComponentCreate>(this);
				m_Owner.GetRegistry().on_destroy<Components::Entity::Rendering>().connect<&RenderSync::OnRenderComponentDestroy>(this);

				m_RendererHandle = Graphics::MasterRenderer::Get().CreateSceneRenderer(std::move(createInfo));
				m_PendingRemovals.reserve(512);
			}

			bool RenderSync::SubmitForRendering() {
				if (m_FrameSubmitted) {
					return true;
				}

				auto* renderer = Graphics::MasterRenderer::Get().Resolve(m_RendererHandle);
				if (!renderer) {
					return true;
				}

				CORI_CORE_ASSERT(m_Pending, "FrameData is not ready for SubmitForRendering, but the scene renderer is already created.");

				m_Pending->rtcqWatermark = Graphics::RenderThreadCommandQueue::CurrentPushCount();

				bool success = renderer->PushFrameData(m_Pending);
				if (success) {
					m_Pending = nullptr;
					m_FrameSubmitted = true;
					Graphics::RenderThreadWakeup::Wake();
					return true;
				}

				return false;
			}

			void RenderSync::OnRenderComponentCreate(entt::registry& registry, entt::entity entity) {
				if (!registry.all_of<Components::Entity::Internal::RenderComponentDirtyFlag>(entity)) {
					registry.emplace<Components::Entity::Internal::RenderComponentDirtyFlag>(entity);
				}

				Entity e = entt::handle{ registry, entity };
				auto& comp = e.GetComponents<Components::Entity::Rendering>();
				comp.m_Owner = e;
			}

			void RenderSync::OnRenderComponentDestroy(entt::registry& registry, entt::entity entity) {
				Entity e = entt::handle{ registry, entity };
				auto& comp = e.GetComponents<Components::Entity::Rendering>();
				if (comp.m_RenderObjectHandle.GetIndex() != UINT32_MAX && comp.m_RenderObjectHandle.GetVersion() != 0) {
					m_PendingRemovals.emplace_back(comp.m_RenderObjectHandle);
				}
			}
		}
	}
}