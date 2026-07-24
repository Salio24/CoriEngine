#pragma once
#include "VulkanEngine.hpp"
#include "Utility/HashCombine.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanImage {
		public:
			struct CreateInfo {
				const vk::ImageCreateInfo* imageCreateInfo{ nullptr };
				const vma::AllocationCreateInfo* allocationCreateInfo{ nullptr };
				const char* name = "";
			};

			struct ImageViewKey {
				vk::ImageViewCreateFlags flags{};
				vk::ImageViewType type;
				vk::ComponentMapping components{};
				vk::ImageSubresourceRange subresourceRange;

				bool operator==(const ImageViewKey& other) const = default;

				auto operator<=>(const ImageViewKey& other) const = default;

				struct Hasher {
					std::size_t operator()(const ImageViewKey& key) const noexcept {
						uint64_t hash;

						Utility::HashCombine(hash, static_cast<uint32_t>(key.flags));
						Utility::HashCombine(hash, static_cast<uint32_t>(key.type));
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
				};
			};

			[[nodiscard]] static VulkanImage Create(const CreateInfo& info);

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

			[[nodiscard]] std::string_view GetName() const {
				#ifdef DEBUG_BUILD
				return m_Name;
				#else
				return "Name unavailable in release build.";
				#endif
			}

			vk::Image m_Image = nullptr;
			vma::Allocation m_Allocation = nullptr;
			vk::Extent3D m_Extent3D;
			vk::Format m_Format;
			uint32_t m_MipLevels{ 0 };
			uint32_t m_ArrayLayers{ 0 };
		private:
			#ifdef DEBUG_BUILD
			std::string m_Name{ "Unnamed Image" };
			#endif
		};
	}
}
