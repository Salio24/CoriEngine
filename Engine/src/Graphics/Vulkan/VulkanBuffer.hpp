#pragma once
#include "VulkanEngine.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanBuffer {
		public:
			struct CreateInfo {
				vk::BufferCreateInfo* bufferCreateInfo{ nullptr };
				vma::AllocationCreateInfo* allocationCreateInfo{ nullptr };
				const char* name = "";
			};

			[[nodiscard]] static VulkanBuffer Create(const CreateInfo& info);

			void Destroy();

			[[nodiscard]] uint64_t GetRawHandle() const {
				return reinterpret_cast<uint64_t>(static_cast<VkBuffer>(m_Buffer));
			}

			[[nodiscard]] vk::DeviceAddress GetBDA() const {
				return VulkanEngine::GetLogicalDevice().getBufferAddress({ .buffer = m_Buffer });
			}

			[[nodiscard]] std::string_view GetName() {
				#ifdef DEBUG_BUILD
				return m_Name;
				#else
				return "Name unavailable in release build.";
				#endif
			}

			vk::Buffer m_Buffer = nullptr;
			vma::Allocation m_Allocation = nullptr;
			size_t m_Size{ 0 };
		private:
			#ifdef DEBUG_BUILD
			std::string m_Name{ "Unnamed Buffer" };
			#endif
		};
	}
}
