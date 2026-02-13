#pragma once
#include "VulkanImage.hpp"
#include "entt/entity/view.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanImageViewManager {
		public:
			static void Init();

			static void Shutdown();

			static VulkanImageViewManager& Get();

			~VulkanImageViewManager() {
				for (auto& val : m_ImageViews | std::views::values) {
					for (auto& view : val | std::views::values) {
						if (view) {
							VulkanEngine::GetLogicalDevice().destroyImageView(view);
						}
					}
				}
			}

		protected:
			friend VulkanImage;

			[[nodiscard]] static vk::ImageView GetView(const VulkanImage& image, const VulkanImage::ImageViewKey& key) {
				auto& viewCache = Get().m_ImageViews[image.GetRawHandle()];

				auto& view = viewCache[key];

				if (view) {
					return view;
				}

				vk::ImageViewCreateInfo createInfo{
					.flags = key.flags,
					.image = image.m_Image,
					.viewType = key.type,
					.format = image.m_Format,
					.components = key.components,
					.subresourceRange = key.subresourceRange
				};

				auto result = VulkanEngine::GetLogicalDevice().createImageView(&createInfo, nullptr, &view);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create image view for image '{}'. Error: {}", image.m_Name, vk::to_string(result));

				VulkanEngine::SetDebugName(view, std::format("Image view of image '{}', subresource: baseMip '{}', levelCount '{}', baseLayer '{}', layerCount '{}'", image.m_Name, key.subresourceRange.baseMipLevel, key.subresourceRange.levelCount, key.subresourceRange.baseArrayLayer, key.subresourceRange.layerCount));

				return view;
			}

			static void DestroyView(const VulkanImage& image, const VulkanImage::ImageViewKey& key) {
				auto& viewCache = Get().m_ImageViews[image.GetRawHandle()];
				auto it = viewCache.find(key);

				if (it != viewCache.end()) {
					if (it->second) {
						VulkanEngine::GetLogicalDevice().destroyImageView(it->second);
					}

					viewCache.erase(it);
				}
			}

			static void UnregisterImage(const VulkanImage& image) {
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

		private:
			std::unordered_map<uint64_t, std::unordered_map<VulkanImage::ImageViewKey, vk::ImageView, VulkanImage::ImageViewKey::Hasher>> m_ImageViews;

			static std::unique_ptr<VulkanImageViewManager> s_Instance;
		};
	}
}
