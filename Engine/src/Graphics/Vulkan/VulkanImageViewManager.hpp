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

			~VulkanImageViewManager();

		protected:
			friend VulkanImage;

			[[nodiscard]] static vk::ImageView GetView(const VulkanImage& image, const VulkanImage::ImageViewKey& key);

			static void DestroyView(const VulkanImage& image, const VulkanImage::ImageViewKey& key);

			static void UnregisterImage(const VulkanImage& image);

		private:
			std::unordered_map<uint64_t, std::unordered_map<VulkanImage::ImageViewKey, vk::ImageView, VulkanImage::ImageViewKey::Hasher>> m_ImageViews;
			std::mutex m_MapMutex;

			static std::unique_ptr<VulkanImageViewManager> s_Instance;
		};
	}
}
