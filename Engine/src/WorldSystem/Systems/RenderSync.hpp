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

				bool WaitForFrameData();

				bool PrepareFrameData();

				bool SubmitForRendering();

				void RequestResize(const vk::Extent2D extent);

				bool Create(Graphics::SceneRenderer::CreateInfo&& createInfo);

				static constexpr SystemPriority Priority = UINT16_MAX;
			private:
				void OnRenderComponentCreate(entt::registry& registry, entt::entity entity);

				void OnRenderComponentDestroy(entt::registry& registry, entt::entity entity);

				Graphics::SceneRendererHandle m_RendererHandle{ 0 };

				vk::Extent2D m_ViewportExtent{};
				bool m_NewExtent{ false };

				Graphics::FrameData* m_Pending{ nullptr };

				bool m_FrameSubmitted{ false };

				std::vector<Core::Handle<Graphics::RenderObject>> m_PendingRemovals;
			};
		}
	}
}
