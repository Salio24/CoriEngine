#pragma once
#include <backends/imgui_impl_vulkan.h>
#include "MasterRenderer.hpp"
#include "ThumbnailRect.hpp"

namespace Cori {
	namespace Graphics {
		class ThumbnailAtlas {
		public:
			struct Copy {
				vk::Image sourceImage{ nullptr };
				vk::Extent2D sourceExtent{};
				ThumbnailRect rect{};
			};

			static void Init();

			static void Shutdown();

			static ThumbnailAtlas& Get();

			~ThumbnailAtlas();

			[[nodiscard]] static std::optional<ImTextureID> GetTexture();

			[[nodiscard]] static constexpr uint32_t GetExtent() {
				return s_ThumbnailAtlasExtent;
			}

		protected:
			friend MasterRenderer;

			void ExecuteCopies(vk::CommandBuffer cmb, const std::span<const Copy> copies);

		private:
			ThumbnailAtlas() = default;

			[[nodiscard]] bool EnsureAtlas();

			void TransitionAtlas(vk::CommandBuffer cmb, const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout);

			VulkanImage m_Atlas{};
			std::atomic<uint64_t> m_ImGuiDescriptorSet{ 0 };
			bool m_AtlasFailed{ false };
			bool m_AtlasNeedsInitialTransition{ false };

			static std::unique_ptr<ThumbnailAtlas> s_Instance;
		};
	}
}
