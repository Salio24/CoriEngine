#include "VulkanImage.hpp"
#include "VulkanImageViewManager.hpp"

namespace Cori {
	namespace Graphics {
		VulkanImage VulkanImage::Create(const CreateInfo& info) {
			VulkanImage image;
			CORI_CORE_ASSERT(info.imageCreateInfo, "ImageCreateInfo that is null was passed to VulkanImage::Create, image name '{}'", info.name);
			CORI_CORE_ASSERT(info.allocationCreateInfo, "AllocationCreateInfo that is null was passed to VulkanImage::Create, image name '{}'", info.name);
			auto [result, value] = VulkanEngine::GetAllocator().createImage(*info.imageCreateInfo, *info.allocationCreateInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create image '{}'. Error: {}", info.name, vk::to_string(result));

			image.m_Allocation = value.first;
			image.m_Image = value.second;
			image.m_Extent3D = info.imageCreateInfo->extent;
			image.m_Format = info.imageCreateInfo->format;
			image.m_MipLevels = info.imageCreateInfo->mipLevels;
			image.m_ArrayLayers = info.imageCreateInfo->arrayLayers;

			#ifdef DEBUG_BUILD
			if (strcmp(info.name, "") != 0) {
				image.m_Name = info.name;
				VulkanEngine::SetDebugName(image.m_Image, info.name);
			}

			VulkanEngine::GetAllocator().setAllocationName(image.m_Allocation, image.m_Name.c_str());
			#endif

			return image;
		}

		void VulkanImage::Destroy() {
			if (m_Image) {
				VulkanImageViewManager::UnregisterImage(*this);
				VulkanEngine::GetAllocator().destroyImage(m_Image, m_Allocation);
			}

			m_Image = nullptr;

			m_Name = "";
			m_Extent3D = vk::Extent3D();
			m_Format = vk::Format();
			m_MipLevels = 0;
			m_ArrayLayers = 0;
		}

		vk::ImageView VulkanImage::GetView(const ImageViewKey& key) {
			return VulkanImageViewManager::GetView(*this, key);
		}

		void VulkanImage::DestroyView(const ImageViewKey& key) {
			VulkanImageViewManager::DestroyView(*this, key);
		}
		std::size_t VulkanImage::ImageViewKey::Hasher::operator()(const ImageViewKey& key) const noexcept {
			uint64_t hash;

			Utility::HashCombine(hash, static_cast<uint32_t>(key.flags));
			Utility::HashCombine(hash, static_cast<uint32_t>(key.type));
			Utility::HashCombine(hash, static_cast<uint32_t>(key.format));
			Utility::HashCombine(hash, static_cast<uint32_t>(key.components.r));
			Utility::HashCombine(hash, static_cast<uint32_t>(key.components.g));
			Utility::HashCombine(hash, static_cast<uint32_t>(key.components.b));
			Utility::HashCombine(hash, static_cast<uint32_t>(key.components.a));
			Utility::HashCombine(hash, static_cast<uint32_t>(key.subresourceRange.aspectMask));
			Utility::HashCombine(hash, key.subresourceRange.baseMipLevel);
			Utility::HashCombine(hash, key.subresourceRange.levelCount);
			Utility::HashCombine(hash, key.subresourceRange.baseArrayLayer);
			Utility::HashCombine(hash, key.subresourceRange.layerCount);

			return hash;
		}

		vk::ImageAspectFlags VulkanImage::GetAspectMask() const {
			if (vk::hasDepthComponent(m_Format) || vk::hasStencilComponent(m_Format)) {
				vk::ImageAspectFlags aspect;
				if (vk::hasDepthComponent(m_Format)) {
					aspect |= vk::ImageAspectFlagBits::eDepth;
				}

				if (vk::hasStencilComponent(m_Format)) {
					aspect |= vk::ImageAspectFlagBits::eStencil;
				}

				return aspect;
			}

			return vk::ImageAspectFlagBits::eColor;
		}

	}
}
