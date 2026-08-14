#include "RenderSync.hpp"
#include "WorldSystem/Components.hpp"
#include "Core/Application.hpp"
#include "Graphics/Vulkan/Renderer/FrameData.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			RenderSync::~RenderSync() {
				Graphics::MasterRenderer::Get().DestroySceneRenderer(m_RendererHandle);
			}

			void RenderSync::PrepareFrameData() {
				if (m_Asleep || m_Pending) {
					return;
				}

				auto* renderer = Graphics::MasterRenderer::Get().Resolve(m_RendererHandle);
				if (!renderer) {
					return;
				}

				Graphics::FrameData* fd = renderer->PopRecycledFrameData();
				CORI_CORE_ASSERT(fd, "SceneRenderer had no recycled frame data available when RenderSync::PrepareFrameData was called.");

				fd->Clear();

				auto view1 = m_Owner.StaticView<Components::Entity::Rendering, Components::Entity::Transform, Components::Entity::Internal::TransformDirtyForRendererFlag, Components::Entity::Internal::RenderComponentDirtyFlag>();

				auto view2 = m_Owner.StaticView<Components::Entity::Rendering, Components::Entity::Transform, Components::Entity::Internal::RenderComponentDirtyFlag>(Exclude<Components::Entity::Internal::TransformDirtyForRendererFlag>());

				auto view3 = m_Owner.StaticView<Components::Entity::Rendering, Components::Entity::Transform, Components::Entity::Internal::TransformDirtyForRendererFlag>(Exclude<Components::Entity::Internal::RenderComponentDirtyFlag>());

				for (auto e : view1) {
					auto& rc = view1.Get<Components::Entity::Rendering>(e);
					auto& tc = view1.Get<Components::Entity::Transform>(e);

					Graphics::Patch patch;
					if (rc.m_NewRegister) {
						rc.m_NewRegister = false;
						rc.m_RenderObjectHandle = renderer->AllocateRenderObjectHandle();
						patch.isRegisterRequest = true;
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

					patch.transform = tc.m_WorldTransform;

					patch.isNewTransform = true;

					patch.handle = rc.m_RenderObjectHandle;
					patch.entityID = e.GetEUID();
					fd->patches.emplace_back(patch);
				}

				for (auto e : view2) {
					auto& rc = view2.Get<Components::Entity::Rendering>(e);

					Graphics::Patch patch;

					if (rc.m_NewRegister) {
						rc.m_NewRegister = false;
						rc.m_RenderObjectHandle = renderer->AllocateRenderObjectHandle();
						auto& tc = view2.Get<Components::Entity::Transform>(e);
						patch.transform = tc.m_WorldTransform;
						patch.isNewTransform = true;
						patch.isRegisterRequest = true;
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

					patch.handle = rc.m_RenderObjectHandle;
					patch.entityID = e.GetEUID();
					fd->patches.emplace_back(patch);
				}

				for (auto e : view3) {
					auto& rc = view3.Get<Components::Entity::Rendering>(e);
					auto& tc = view3.Get<Components::Entity::Transform>(e);

					Graphics::Patch patch;

					patch.transform = tc.m_WorldTransform;
					patch.isNewTransform = true;

					patch.handle = rc.m_RenderObjectHandle;
					patch.entityID = e.GetEUID();
					fd->patches.emplace_back(patch);
				}

				const auto& camera = m_Owner.GetActiveCamera();
				fd->cameraSnapshot = Graphics::CameraSnapshot{
					.view = camera.GetViewMatrix(),
					.projection = camera.GetProjectionMatrix(),
					.viewportSize = m_ViewportExtent
				};

				if (m_NewExtent) {
					fd->resizeRequest = m_ViewportExtent;
					m_NewExtent = false;
				}

				if (m_PendingThumbnailCopy) {
					fd->thumbnailCopy = m_PendingThumbnailCopy;
					m_PendingThumbnailCopy.reset();
				}

				if (m_PendingPick) {
					fd->pickRequest = m_PendingPick;
					m_PendingPick.reset();
				}

				fd->highlights = m_Highlights;

				fd->deletedObjects.swap(m_PendingRemovals);
				m_PendingRemovals.clear();

				m_Owner.Clear<Components::Entity::Internal::RenderComponentDirtyFlag, Components::Entity::Internal::TransformDirtyForRendererFlag>();

				m_Pending = fd;
			}

			bool RenderSync::Create(Graphics::SceneRenderer::CreateInfo&& createInfo) {
				m_ViewportExtent = createInfo.initialPRTExtent;

				m_Owner.GetRegistry().on_construct<Components::Entity::Rendering>().connect<&RenderSync::OnRenderComponentCreate>(this);
				m_Owner.GetRegistry().on_destroy<Components::Entity::Rendering>().connect<&RenderSync::OnRenderComponentDestroy>(this);

				m_RendererHandle = Graphics::MasterRenderer::Get().CreateSceneRenderer(std::move(createInfo));
				m_PendingRemovals.reserve(512);
				return true;
			}

			void RenderSync::SubmitForRendering() {
				if (!m_Pending) {
					return;
				}

				auto* renderer = Graphics::MasterRenderer::Get().Resolve(m_RendererHandle);
				if (!renderer) {
					return;
				}

				[[maybe_unused]] const bool success = renderer->PushFrameData(m_Pending);
				CORI_CORE_ASSERT(success, "SceneRenderer ready ring was full when RenderSync::SubmitForRendering was called.");

				m_Pending = nullptr;

				Graphics::MasterRenderer::Get().AddParticipant(m_RendererHandle);
			}

			void RenderSync::Sleep() {
				m_Asleep = true;
			}

			void RenderSync::WakeUp() {
				m_Asleep = false;
			}

			bool RenderSync::IsAsleep() const {
				return m_Asleep;
			}

			void RenderSync::RequestResize(const vk::Extent2D extent) {
				m_ViewportExtent = extent;
				m_NewExtent = true;
			}

			void RenderSync::RequestThumbnailCopy(const Graphics::ThumbnailRect rect) {
				m_PendingThumbnailCopy = rect;
			}

			uint64_t RenderSync::RequestPick(const float u, const float v) {
				const uint64_t ticket = m_NextPickTicket++;
				m_PendingPick = Graphics::PickRequest{ .ticket = ticket, .u = u, .v = v };
				return ticket;
			}

			void RenderSync::ClearHighlights() {
				m_Highlights.clear();
			}

			void RenderSync::AddHighlight(const entt::entity entity, const uint32_t color) {
				if (entity == entt::null || !m_Owner.GetRegistry().valid(entity)) {
					return;
				}

				const auto* rc = m_Owner.GetRegistry().try_get<Components::Entity::Rendering>(entity);
				if (!rc || rc->m_RenderObjectHandle.GetIndex() == UINT32_MAX) {
					return;
				}

				m_Highlights.emplace_back(Graphics::HighlightRequest{ .color = color, .renderObjectIndex = rc->m_RenderObjectHandle.GetIndex() });
			}

			bool RenderSync::PollPickResult(Graphics::PickResult& result) {
				auto* renderer = Graphics::MasterRenderer::Get().Resolve(m_RendererHandle);
				if (!renderer) {
					return false;
				}

				return renderer->PollPickResult(result);
			}

			uint64_t RenderSync::GetThumbnailCopyCount() const {
				const auto* renderer = Graphics::MasterRenderer::Get().Resolve(m_RendererHandle);
				if (renderer) {
					return renderer->GetThumbnailCopyCount();
				}

				return 0;
			}

			std::optional<ImTextureID> RenderSync::GetMainPRT() const {
				auto* renderer = Graphics::MasterRenderer::Get().Resolve(m_RendererHandle);
				if (!renderer) {
					return std::nullopt;
				}

				VkDescriptorSet set = renderer->GetPRT().GetImGuiDescriptorSet();
				if (std::bit_cast<uint64_t>(set) == 0) {
					return std::nullopt;
				}

				return reinterpret_cast<ImTextureID>(set);
			}

			Graphics::SceneRendererHandle RenderSync::GetRendererHandle() const {
				return m_RendererHandle;
			}

			void RenderSync::Bind() {
				Graphics::MasterRenderer::ChangeMainRenderer(m_RendererHandle);
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
