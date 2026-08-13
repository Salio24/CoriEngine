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
				vk::Format format{ vk::Format::eUndefined };
				vk::ComponentMapping components{};
				vk::ImageSubresourceRange subresourceRange;

				bool operator==(const ImageViewKey& other) const = default;

				auto operator<=>(const ImageViewKey& other) const = default;

				struct Hasher {
					std::size_t operator()(const ImageViewKey& key) const noexcept;
				};
			};

			[[nodiscard]] static VulkanImage Create(const CreateInfo& info);

			void Destroy();

			[[nodiscard]] vk::ImageView GetView(const ImageViewKey& key);

			void DestroyView(const ImageViewKey& key);

			[[nodiscard]] vk::ImageAspectFlags GetAspectMask() const;

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
