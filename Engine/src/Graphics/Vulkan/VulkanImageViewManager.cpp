#include "VulkanImageViewManager.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanImageViewManager> VulkanImageViewManager::s_Instance{ nullptr };

		void VulkanImageViewManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanImageViewManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanImageViewManager>(new VulkanImageViewManager());
		}

		void VulkanImageViewManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanImageViewManager& VulkanImageViewManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanImageViewManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
		VulkanImageViewManager::~VulkanImageViewManager() {
			for (auto& val : m_ImageViews | std::views::values) {
				for (auto& view : val | std::views::values) {
					if (view) {
						VulkanEngine::GetLogicalDevice().destroyImageView(view);
					}
				}
			}
		}

		vk::ImageView VulkanImageViewManager::GetView(const VulkanImage& image, const VulkanImage::ImageViewKey& key) {
			std::lock_guard lk(Get().m_MapMutex);
			auto& viewCache = Get().m_ImageViews[image.GetRawHandle()];

			auto& view = viewCache[key];

			if (view) {
				return view;
			}

			vk::ImageViewCreateInfo createInfo{
				.flags = key.flags,
				.image = image.m_Image,
				.viewType = key.type,
				.format = key.format == vk::Format::eUndefined ? image.m_Format : key.format,
				.components = key.components,
				.subresourceRange = key.subresourceRange
			};

			auto result = VulkanEngine::GetLogicalDevice().createImageView(&createInfo, nullptr, &view);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create image view for image '{}'. Error: {}", image.GetName(), vk::to_string(result));

			VulkanEngine::SetDebugName(view, std::format("Image view of image '{}', subresource: baseMip '{}', levelCount '{}', baseLayer '{}', layerCount '{}'", image.GetName(), key.subresourceRange.baseMipLevel, key.subresourceRange.levelCount, key.subresourceRange.baseArrayLayer, key.subresourceRange.layerCount));

			return view;
		}

		void VulkanImageViewManager::DestroyView(const VulkanImage& image, const VulkanImage::ImageViewKey& key) {
			std::lock_guard lk(Get().m_MapMutex);
			auto& viewCache = Get().m_ImageViews[image.GetRawHandle()];
			auto it = viewCache.find(key);

			if (it != viewCache.end()) {
				if (it->second) {
					VulkanEngine::GetLogicalDevice().destroyImageView(it->second);
				}

				viewCache.erase(it);
			}
		}

		void VulkanImageViewManager::UnregisterImage(const VulkanImage& image) {
			std::lock_guard lk(Get().m_MapMutex);
			auto it = Get().m_ImageViews.find(image.GetRawHandle());
			if (it != Get().m_ImageViews.end()) {
				for (auto& view : it->second | std::views::values) {
					if (view) {
						VulkanEngine::GetLogicalDevice().destroyImageView(view);
					}
				}

				Get().m_ImageViews.erase(it);
			}
		}

	}
}