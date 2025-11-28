#include "VulkanImage.hpp"
#include "VulkanResourceTracker.hpp"
#include "VulkanImageViewManager.hpp"

namespace Cori {
	namespace Graphics {
		VulkanImage VulkanImage::Create(CreateInfo& info) {
			VulkanImage image;
			auto [result, value] = VulkanEngine::GetAllocator().createImage(*info.imageCreateInfo, *info.allocationCreateInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create image '{}'. Error: {}", info.name, vk::to_string(result));

			image.m_Allocation = value.first;
			image.m_Image = value.second;
			image.m_Extent3D = info.imageCreateInfo->extent;
			image.m_Format = info.imageCreateInfo->format;
			image.m_MipLevels = info.imageCreateInfo->mipLevels;
			image.m_ArrayLayers = info.imageCreateInfo->arrayLayers;
			image.m_InitialLayout = info.imageCreateInfo->initialLayout;
			VulkanResourceTracker::RegisterImage(image);

			if (strcmp(info.name, "") != 0) {
				image.m_Name = info.name;
				VulkanEngine::SetDebugName(image.m_Image, info.name);
			}

			return image;
		}

		void VulkanImage::Destroy() {
			VulkanResourceTracker::UnregisterImage(*this);
			VulkanImageViewManager::UnregisterImage(*this);

			VulkanEngine::GetAllocator().destroyImage(m_Image, m_Allocation);

			m_Image = VK_NULL_HANDLE;

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
	}
}
