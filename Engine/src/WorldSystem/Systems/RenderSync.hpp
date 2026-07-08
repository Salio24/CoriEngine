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

				bool PrepareFrameData();

				bool SubmitForRendering();

				bool Create(Graphics::SceneRenderer::CreateInfo&& createInfo);

				static constexpr SystemPriority Priority = UINT16_MAX;
			private:
				void OnRenderComponentCreate(entt::registry& registry, entt::entity entity);

				void OnRenderComponentDestroy(entt::registry& registry, entt::entity entity);

				Graphics::SceneRendererHandle m_RendererHandle{ 0 };

				Graphics::FrameData* m_Pending{ nullptr };

				bool m_FrameSubmitted{ false };

				std::vector<Core::Handle<Graphics::RenderObject>> m_PendingRemovals;
			};
		}
	}
}
