#pragma once
#include "VulkanEngine.hpp"
#include "DeletionQueue.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanImage {
		public:
			struct CreateInfo {
				vk::ImageCreateInfo* imageCreateInfo{ nullptr };
				vma::AllocationCreateInfo* allocationCreateInfo{ nullptr };
				const char* name = "";
			};

			struct ImageViewKey {
				vk::ImageViewCreateFlags flags;
				vk::ImageViewType type;
				vk::ComponentMapping components;
				vk::ImageSubresourceRange subresourceRange;

				bool operator==(const ImageViewKey& other) const = default;

				auto operator<=>(const ImageViewKey& other) const = default;
			};


			[[nodiscard]] static VulkanImage Create(CreateInfo& info);

			void Destroy();

			[[nodiscard]] vk::ImageView GetView(const ImageViewKey& key);

			void DestroyView(const ImageViewKey& key);

			[[nodiscard]] vk::ImageAspectFlags GetAspectMask() const {
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

			[[nodiscard]] uint64_t GetRawHandle() const {
				return reinterpret_cast<uint64_t>(static_cast<VkImage>(m_Image));
			}

			vk::Image m_Image = nullptr;
			vma::Allocation m_Allocation = nullptr;
			vk::Extent3D m_Extent3D;
			vk::Format m_Format;
			vk::ImageLayout m_InitialLayout;
			uint32_t m_MipLevels{ 0 };
			uint32_t m_ArrayLayers{ 0 };
			const char* m_Name{ "Unnamed Image" };
		};
	}
}
