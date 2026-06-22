#pragma once
#include "Graphics/Vulkan/VulkanEngine.hpp"

namespace Cori {
	namespace Graphics {
		class ImGuiRenderer {
		public:
			static void Init(void* window);

			static void Shutdown();

			static void StartFrame();

			static void Render(vk::CommandBuffer cmb, vk::ImageView swapChainImageView, const vk::Extent2D swapChainExtent, const bool frameSkip);
		};
	}
}
