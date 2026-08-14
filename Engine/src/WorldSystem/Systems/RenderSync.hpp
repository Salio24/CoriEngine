#pragma once
#include "System.hpp"
#include "Graphics/Vulkan/Renderer/SceneRenderer.hpp"
#include "Graphics/Vulkan/Renderer/MasterRenderer.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class RenderSync final : public System {
			public:
				~RenderSync() override;

				void PrepareFrameData();

				void SubmitForRendering();

				void Sleep();

				void WakeUp();

				[[nodiscard]] bool IsAsleep() const;

				void RequestResize(const vk::Extent2D extent);

				void RequestThumbnailCopy(const Graphics::ThumbnailRect rect);

				[[nodiscard]] uint64_t GetThumbnailCopyCount() const;

				[[nodiscard]] uint64_t RequestPick(const float u, const float v);

				[[nodiscard]] bool PollPickResult(Graphics::PickResult& result);

				void ClearHighlights();

				void AddHighlight(const entt::entity entity, const uint32_t color);

				[[nodiscard]] std::optional<ImTextureID> GetMainPRT() const;

				[[nodiscard]] Graphics::SceneRendererHandle GetRendererHandle() const;

				void Bind();

				bool Create(Graphics::SceneRenderer::CreateInfo&& createInfo);

				static constexpr SystemPriority Priority = UINT16_MAX;
			private:
				void OnRenderComponentCreate(entt::registry& registry, entt::entity entity);

				void OnRenderComponentDestroy(entt::registry& registry, entt::entity entity);

				Graphics::SceneRendererHandle m_RendererHandle{ 0 };

				vk::Extent2D m_ViewportExtent{};
				bool m_NewExtent{ false };

				std::optional<Graphics::ThumbnailRect> m_PendingThumbnailCopy;

				std::optional<Graphics::PickRequest> m_PendingPick;
				uint64_t m_NextPickTicket{ 1 };

				std::vector<Graphics::HighlightRequest> m_Highlights;

				Graphics::FrameData* m_Pending{ nullptr };

				bool m_Asleep{ false };

				std::vector<Core::Handle<Graphics::RenderObject>> m_PendingRemovals;
			};
		}
	}
}
