#pragma once
#include <backends/imgui_impl_vulkan.h>
#include "Graphics/Vulkan/DeletionQueue.hpp"
#include "Graphics/Vulkan/VulkanImage.hpp"

namespace Cori {
	namespace Graphics {
		class PersistentRenderTarget {
		public:
			PersistentRenderTarget(const PersistentRenderTarget&) = delete;
			PersistentRenderTarget& operator=(const PersistentRenderTarget&) = delete;
			PersistentRenderTarget(PersistentRenderTarget&&) = delete;
			PersistentRenderTarget& operator=(PersistentRenderTarget&&) = delete;

			PersistentRenderTarget(const vk::Extent2D extent, const vk::Format format, const bool registerWithImGui, const char* name = "");

			~PersistentRenderTarget();

			VulkanImage& GetImage() {
				return m_Image;
			}

			vk::ImageView GetImageView() {
				return m_ImageView;
			}

			void Resize(vk::Extent2D extent);

			void InitialRegisterWithImGui();

			[[nodiscard]] VkDescriptorSet GetImGuiDescriptorSet() const {
				return reinterpret_cast<VkDescriptorSet>(m_ImGuiDescriptorSet.load(std::memory_order_acquire));
			}

			void PushImageForInitialTransition();

		private:
			VulkanImage m_Image;
			vk::Format m_Format;
			vk::ImageView m_ImageView;
			std::atomic<uint64_t> m_ImGuiDescriptorSet{ 0 };
			std::atomic<bool> m_IsDirectlyBlit{ false };
			bool m_RegisterWithImGui{ false };

			#ifdef DEBUG_BUILD
			std::string m_Name{ "Unnamed PRT" };
			#endif
		};
	}
}