#include "VulkanLayoutManager.hpp"
namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanGlobalLayoutManager> VulkanGlobalLayoutManager::s_Instance{ nullptr };

		void VulkanGlobalLayoutManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanGlobalLayoutManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanGlobalLayoutManager>(new VulkanGlobalLayoutManager());
		}

		void VulkanGlobalLayoutManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanGlobalLayoutManager& VulkanGlobalLayoutManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanGlobalLayoutManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
		void VulkanGlobalLayoutManager::UpdateSamplerDescriptor(const uint32_t slot, const vk::Sampler sampler) {
			vk::DescriptorGetInfoEXT getInfo {
				.type = vk::DescriptorType::eSampler,
				.data = vk::DescriptorDataEXT(&sampler)
			};

			uint64_t descriptorSize = Get().m_PDDBP.samplerDescriptorSize;

			std::vector<Byte> payload(descriptorSize);

			VulkanEngine::GetLogicalDevice().getDescriptorEXT(&getInfo, descriptorSize, payload.data());
			Get().m_DescriptorBuffer.InsertRange(payload, Get().m_SamplerBindingMemOffset + slot * descriptorSize);
		}

		void VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(const uint32_t slot, const vk::ImageView view) {
			vk::DescriptorImageInfo imageInfo{
				.imageView = view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			};

			vk::DescriptorGetInfoEXT getInfo {
				.type = vk::DescriptorType::eSampledImage,
				.data = vk::DescriptorDataEXT(&imageInfo)
			};

			uint64_t descriptorSize = Get().m_PDDBP.sampledImageDescriptorSize;

			std::vector<Byte> payload(descriptorSize);

			VulkanEngine::GetLogicalDevice().getDescriptorEXT(&getInfo, descriptorSize, payload.data());

			Get().m_DescriptorBuffer.InsertRange(payload, Get().m_SampledImageBindingMemOffset + slot * descriptorSize);
		}

		void VulkanGlobalLayoutManager::BindDescriptorBuffer(vk::CommandBuffer& cmb) {
			vk::DescriptorBufferBindingInfoEXT bindInfo {
				.address = Get().m_DescriptorBuffer.GetVulkanBuffer().GetBDA(),
				.usage = vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT | vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT
			};

			cmb.bindDescriptorBuffersEXT(bindInfo);

			uint32_t bufferIndex = 0;
			vk::DeviceSize setOffset = 0;

			cmb.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, Get().m_PipelineLayout, 0, 1, &bufferIndex, &setOffset);
		}

		VulkanGlobalLayoutManager::~VulkanGlobalLayoutManager() {
			VulkanEngine::GetLogicalDevice().destroyPipelineLayout(m_PipelineLayout);
			VulkanEngine::GetLogicalDevice().destroyDescriptorSetLayout(m_DescriptorSetLayout);
		}

		VulkanGlobalLayoutManager::VulkanGlobalLayoutManager() {
			std::array bindings {
				vk::DescriptorSetLayoutBinding{
					.binding = 0,
					.descriptorType = vk::DescriptorType::eSampler,
					.descriptorCount = s_MaxSamplers,
					.stageFlags = vk::ShaderStageFlagBits::eFragment
				},
				vk::DescriptorSetLayoutBinding{
					.binding = 1,
					.descriptorType = vk::DescriptorType::eSampledImage,
					.descriptorCount = s_MaxTextures,
					.stageFlags = vk::ShaderStageFlagBits::eFragment
				}
			};

			vk::DescriptorSetLayoutCreateInfo createInfo {
				.flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
				.bindingCount = bindings.size(),
				.pBindings = bindings.data()
			};

			auto [result, setLayout] = VulkanEngine::GetLogicalDevice().createDescriptorSetLayout(createInfo);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create global descriptor set layout. Error: {}", vk::to_string(result));

			m_PushConstantRange.size = 128;
			m_PushConstantRange.stageFlags = vk::ShaderStageFlagBits::eAll;

			vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
				.setLayoutCount = 1,
				.pSetLayouts = &setLayout,
				.pushConstantRangeCount = 1,
				.pPushConstantRanges = &m_PushConstantRange
			};

			auto [result_, pipelineLayout] = VulkanEngine::GetLogicalDevice().createPipelineLayout(pipelineLayoutCreateInfo);
			CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create global pipeline layout. Error: {}", vk::to_string(result_));

			m_DescriptorSetLayout = setLayout;
			m_PipelineLayout = pipelineLayout;

			VulkanEngine::SetDebugName(m_DescriptorSetLayout, "Global descriptor set layout");
			VulkanEngine::SetDebugName(m_PipelineLayout, "Global pipeline layout");

			vk::DeviceSize setSize = VulkanEngine::GetLogicalDevice().getDescriptorSetLayoutSizeEXT(m_DescriptorSetLayout);

			vk::PhysicalDeviceProperties2 props;
			props.pNext = &m_PDDBP;

			VulkanEngine::GetPhysicalDevice().getProperties2(&props);

			uint64_t alignedSize = Math::AlignUp(setSize, m_PDDBP.descriptorBufferOffsetAlignment);

			m_SamplerBindingMemOffset = VulkanEngine::GetLogicalDevice().getDescriptorSetLayoutBindingOffsetEXT(m_DescriptorSetLayout, 0);
			m_SampledImageBindingMemOffset = VulkanEngine::GetLogicalDevice().getDescriptorSetLayoutBindingOffsetEXT(m_DescriptorSetLayout, 1);

			m_DescriptorBuffer.Resize(alignedSize);
		}

	}
}