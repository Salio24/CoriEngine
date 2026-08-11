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

			[[nodiscard]] static std::optional<ImTextureID> GetTexture() {
				const uint64_t raw = Get().m_ImGuiDescriptorSet.load(std::memory_order_acquire);
				if (raw == 0) {
					return std::nullopt;
				}

				return reinterpret_cast<ImTextureID>(reinterpret_cast<VkDescriptorSet>(raw));
			}

			[[nodiscard]] static constexpr uint32_t GetExtent() {
				return s_ThumbnailAtlasExtent;
			}

		protected:
			friend MasterRenderer;

			void ExecuteCopies(vk::CommandBuffer cmb, const std::span<const Copy> copies) {
				if (copies.empty() || !EnsureAtlas()) {
					return;
				}

				CORI_VK_LABEL_F(cmb, DebugLabelColors::Transfer, "Copy {} thumbnail(s) into the atlas", copies.size());

				TransitionAtlas(cmb, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal);

				for (const Copy& copy : copies) {
					const std::array srcOffsets{
						vk::Offset3D{ 0, 0, 0 },
						vk::Offset3D{ static_cast<int32_t>(copy.sourceExtent.width), static_cast<int32_t>(copy.sourceExtent.height), 1 }
					};

					const std::array dstOffsets{
						vk::Offset3D{ static_cast<int32_t>(copy.rect.x), static_cast<int32_t>(copy.rect.y), 0 },
						vk::Offset3D{ static_cast<int32_t>(copy.rect.x + copy.rect.size), static_cast<int32_t>(copy.rect.y + copy.rect.size), 1 }
					};

					const vk::ImageBlit blit{
						.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.srcOffsets = srcOffsets,
						.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.dstOffsets = dstOffsets
					};

					cmb.blitImage(copy.sourceImage, vk::ImageLayout::eTransferSrcOptimal, m_Atlas.m_Image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);
				}

				TransitionAtlas(cmb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
			}

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
