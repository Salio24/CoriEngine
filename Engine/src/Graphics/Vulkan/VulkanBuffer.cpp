#include "VulkanBuffer.hpp"

namespace Cori {
	namespace Graphics {
		VulkanBuffer VulkanBuffer::Create(const CreateInfo& info) {
			VulkanBuffer buffer;
			CORI_CORE_ASSERT(info.bufferCreateInfo, "BufferCreateInfo that is null was passed to VulkanBuffer::Create, buffer name '{}'", info.name);
			CORI_CORE_ASSERT(info.allocationCreateInfo, "AllocationCreateInfo that is null was passed to VulkanBuffer::Create, buffer name '{}'", info.name);
			auto [result, value] = VulkanEngine::GetAllocator().createBuffer(*info.bufferCreateInfo, *info.allocationCreateInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create buffer '{}'. Error: {}", info.name, vk::to_string(result));

			buffer.m_Allocation = value.first;
			buffer.m_Buffer = value.second;
			buffer.m_Size = info.bufferCreateInfo->size;

			#ifdef DEBUG_BUILD
			if (strcmp(info.name, "") != 0) {
				buffer.m_Name = info.name;
				VulkanEngine::SetDebugName(buffer.m_Buffer, info.name);
			}
			#endif

			return buffer;
		}

		void VulkanBuffer::Destroy() {
			VulkanEngine::GetAllocator().destroyBuffer(m_Buffer, m_Allocation);
			m_Buffer = nullptr;
			m_Size = 0;
		}
	}
}