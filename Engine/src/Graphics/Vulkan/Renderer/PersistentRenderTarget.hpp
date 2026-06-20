#pragma once
#include <backends/imgui_impl_vulkan.h>
#include "Graphics/Vulkan/VulkanImage.hpp"

namespace Cori {
	namespace Graphics {
		class PersistentRenderTarget {
		public:
			PersistentRenderTarget(const PersistentRenderTarget&) = delete;
			PersistentRenderTarget(PersistentRenderTarget&&) = delete;
			PersistentRenderTarget& operator=(const PersistentRenderTarget&) = delete;
			PersistentRenderTarget& operator=(PersistentRenderTarget&&) = delete;

			PersistentRenderTarget(const vk::Extent2D extent, const vk::Format format, const bool registerWithImGui, const char* name = "") : m_Format(format), m_RegisterWithImGui(registerWithImGui) {
				vk::ImageCreateInfo imageInfo{
					.imageType = vk::ImageType::e2D,
					.format = m_Format,
					.extent = { extent.width, extent.height, 1 },
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = vk::SampleCountFlagBits::e1,
					.tiling = vk::ImageTiling::eOptimal,
					.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					.sharingMode = vk::SharingMode::eExclusive,
					.initialLayout = vk::ImageLayout::eUndefined
				};

				vma::AllocationCreateInfo allocInfo{
					.usage = vma::MemoryUsage::eAuto,
					.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
				};

				VulkanImage::CreateInfo info{
					.imageCreateInfo = &imageInfo,
					.allocationCreateInfo = &allocInfo
				};

				#ifdef DEBUG_BUILD
				if (!CORI_IS_EMPTY_CSTR(name)) {
					m_Name = std::string(name);
				}

				info.name = m_Name.c_str();
				#endif

				m_Image = VulkanImage::Create(info);
			}

			~PersistentRenderTarget() {
				DeletionQueue::PushImage(m_Image, FRAMES_IN_FLIGHT * 2 + 1);
				uint64_t raw = m_ImGuiDescriptorSet.load(std::memory_order_relaxed);
				if (raw != 0) {
					DeletionQueue::PushDeleter([raw]{ ImGui_ImplVulkan_RemoveTexture(std::bit_cast<VkDescriptorSet>(raw)); }, FRAMES_IN_FLIGHT * 2 + 1);
				}
			}

			VulkanImage& GetImage() {
				return m_Image;
			}

			void Resize(vk::Extent2D extent) {
				if (extent.width == m_Image.m_Extent3D.width && extent.height == m_Image.m_Extent3D.height) {
					return;
				}

				DeletionQueue::PushImage(m_Image, FRAMES_IN_FLIGHT * 2 + 1);
				uint64_t raw = m_ImGuiDescriptorSet.load(std::memory_order_relaxed);
				if (raw != 0) {
					DeletionQueue::PushDeleter([raw]{ ImGui_ImplVulkan_RemoveTexture(std::bit_cast<VkDescriptorSet>(raw)); }, FRAMES_IN_FLIGHT * 2 + 1);
				}

				vk::ImageCreateInfo imageInfo{
					.imageType = vk::ImageType::e2D,
					.format = m_Format,
					.extent = { extent.width, extent.height, 1 },
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = vk::SampleCountFlagBits::e1,
					.tiling = vk::ImageTiling::eOptimal,
					.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					.sharingMode = vk::SharingMode::eExclusive,
					.initialLayout = vk::ImageLayout::eUndefined
				};

				vma::AllocationCreateInfo allocInfo{
					.usage = vma::MemoryUsage::eAuto,
					.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
				};

				VulkanImage::CreateInfo info{
					.imageCreateInfo = &imageInfo,
					.allocationCreateInfo = &allocInfo
				};

				#ifdef DEBUG_BUILD
				info.name = m_Name.c_str();
				#endif

				m_Image = VulkanImage::Create(info);

				if (m_RegisterWithImGui) {
					VulkanImage::ImageViewKey viewKey{
						.type = vk::ImageViewType::e2D,
						.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
					};

					vk::ImageView view = m_Image.GetView(viewKey);
					VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(view, static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
					m_ImGuiDescriptorSet.store(std::bit_cast<uint64_t>(set), std::memory_order_release);
				}
			}

			void InitialRegisterWithImGui() {
				if (m_RegisterWithImGui && m_ImGuiDescriptorSet.load(std::memory_order_relaxed) == 0) {
					VulkanImage::ImageViewKey viewKey{
						.type = vk::ImageViewType::e2D,
						.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
					};

					vk::ImageView view = m_Image.GetView(viewKey);
					VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(view, static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
					m_ImGuiDescriptorSet.store(std::bit_cast<uint64_t>(set), std::memory_order_release);
				}
			}

			VkDescriptorSet GetImGuiDescriptorSet() const {
				return std::bit_cast<VkDescriptorSet>(m_ImGuiDescriptorSet.load(std::memory_order_acquire));
			}

		private:
			VulkanImage m_Image;
			vk::Format m_Format;
			std::atomic<uint64_t> m_ImGuiDescriptorSet{ 0 };
			std::atomic<bool> m_IsDirectlyBlit{ false };
			bool m_RegisterWithImGui{ false };

			#ifdef DEBUG_BUILD
			std::string m_Name{ "Unnamed PRT" };
			#endif
		};
	}
}