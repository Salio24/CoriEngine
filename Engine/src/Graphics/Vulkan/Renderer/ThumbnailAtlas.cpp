#include "ThumbnailAtlas.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<ThumbnailAtlas> ThumbnailAtlas::s_Instance{ nullptr };

		void ThumbnailAtlas::Init() {
			CORI_CORE_ASSERT(!s_Instance, "ThumbnailAtlas is already initialized.");
			s_Instance = std::unique_ptr<ThumbnailAtlas>(new ThumbnailAtlas());
		}

		void ThumbnailAtlas::Shutdown() {
			s_Instance.reset();
		}

		ThumbnailAtlas& ThumbnailAtlas::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling ThumbnailAtlas::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		ThumbnailAtlas::~ThumbnailAtlas() {
			uint64_t raw = m_ImGuiDescriptorSet.exchange(0, std::memory_order_acq_rel);
			if (raw != 0) {
				ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(raw));
			}

			if (m_Atlas.m_Image) {
				m_Atlas.Destroy();
			}
		}

		bool ThumbnailAtlas::EnsureAtlas() {
			if (m_Atlas.m_Image) {
				return true;
			}

			if (m_AtlasFailed) {
				return false;
			}

			vk::ImageCreateInfo imageInfo{
				.imageType = vk::ImageType::e2D,
				.format = vk::Format::eR8G8B8A8Unorm,
				.extent = { s_ThumbnailAtlasExtent, s_ThumbnailAtlasExtent, 1 },
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = vk::SampleCountFlagBits::e1,
				.tiling = vk::ImageTiling::eOptimal,
				.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
				.sharingMode = vk::SharingMode::eExclusive,
				.initialLayout = vk::ImageLayout::eUndefined
			};

			vma::AllocationCreateInfo allocInfo{
				.usage = vma::MemoryUsage::eAuto,
				.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
			};

			VulkanImage::CreateInfo createInfo{
				.imageCreateInfo = &imageInfo,
				.allocationCreateInfo = &allocInfo,
				.name = "Thumbnail Atlas"
			};

			m_Atlas = VulkanImage::Create(createInfo);

			if (!m_Atlas.m_Image) {
				CORI_CORE_ERROR("ThumbnailAtlas failed to allocate its {}x{} atlas image, thumbnails are disabled.", s_ThumbnailAtlasExtent, s_ThumbnailAtlasExtent);
				m_AtlasFailed = true;
				return false;
			}

			m_AtlasNeedsInitialTransition = true;

			CORI_CORE_INFO("[Thumbnail] atlas allocated: {}x{}", s_ThumbnailAtlasExtent, s_ThumbnailAtlasExtent);

			VulkanImage::ImageViewKey viewKey{
				.type = vk::ImageViewType::e2D,
				.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
			};

			vk::ImageView view = m_Atlas.GetView(viewKey);
			VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(view, static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));

			m_ImGuiDescriptorSet.store(reinterpret_cast<uint64_t>(set), std::memory_order_release);

			return true;
		}

		void ThumbnailAtlas::TransitionAtlas(vk::CommandBuffer cmb, const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout) {
			bool toTransfer = newLayout == vk::ImageLayout::eTransferDstOptimal;

			vk::ImageLayout effectiveOldLayout = oldLayout;
			if (m_AtlasNeedsInitialTransition && toTransfer) {
				effectiveOldLayout = vk::ImageLayout::eUndefined;
				m_AtlasNeedsInitialTransition = false;
			}

			vk::ImageMemoryBarrier2 barrier{
				.srcStageMask = toTransfer ? vk::PipelineStageFlagBits2::eFragmentShader : vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = toTransfer ? vk::AccessFlagBits2::eShaderSampledRead : vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask = toTransfer ? vk::PipelineStageFlagBits2::eTransfer : vk::PipelineStageFlagBits2::eFragmentShader,
				.dstAccessMask = toTransfer ? vk::AccessFlagBits2::eTransferWrite : vk::AccessFlagBits2::eShaderSampledRead,
				.oldLayout = effectiveOldLayout,
				.newLayout = newLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = m_Atlas.m_Image,
				.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
			};

			CORI_VK_LABEL_INSERT(cmb, "Thumbnail atlas layout transition", DebugLabelColors::Barrier);

			vk::DependencyInfo depInfo{
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &barrier
			};

			cmb.pipelineBarrier2(depInfo);
		}
	}
}
