#pragma once
#include "Graphics/Vulkan/VulkanEngine.hpp"


namespace Cori {
	namespace Graphics {
		class ImGuiRenderer {
		public:
			struct Data;

			static void Init(void* window);

			static void Shutdown();

			static void StartFrame();

			static void EndFrame();

			static void ProcessTexQueueRequests();

			static void Render(vk::CommandBuffer cmb);

			static void RecycleSnapshot();

			static std::unique_ptr<Data> s_Data;
		};
	}
}
