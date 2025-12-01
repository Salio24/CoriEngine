#pragma once
#include "VulkanEngine.hpp"
#include "VulkanUploadManager.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanGlobalLayoutManager {
		public:
			static void Init();

			static void Shutdown();

			static VulkanGlobalLayoutManager& Get();

			static vk::DescriptorSetLayout& GetGlobalDescriptorSetLayout() {
				return Get().m_DescriptorSetLayout;
			}

			static vk::PipelineLayout& GetGlobalPipelineLayout() {
				return Get().m_PipelineLayout;
			}

			static vk::PushConstantRange& GetGlobalPushConstantRange() {
				return Get().m_PushConstantRange;
			}

			static uint16_t GetMaxTextures() {
				return s_MaxTextures;
			}

			static uint16_t GetMaxSamplers() {
				return s_MaxSamplers;
			}

			static void UpdateSamplerDescriptor(const uint32_t slot, const vk::Sampler sampler) {
				vk::DescriptorGetInfoEXT getInfo {
					.type = vk::DescriptorType::eSampler,
					.data = vk::DescriptorDataEXT(&sampler)
				};

				uint64_t descriptorSize = Get().m_PDDBP.samplerDescriptorSize;

				std::vector<Byte> payload(descriptorSize);

				VulkanEngine::GetLogicalDevice().getDescriptorEXT(&getInfo, descriptorSize, payload.data());

				AmazingBuffer::UpdateData updateData {
					.offset = Get().m_SamplerBindingMemOffset + slot * descriptorSize,
					.alignment = descriptorSize,
					.data = std::move(payload)
				};

				auto& db = VulkanUploadManager::GetAmazingBuffer(Get().m_DescriptorBufferHandle);

				db.SubmitUpdate(std::move(updateData));
			}

			static void UpdateSampledTextureDescriptor(const uint32_t slot, const vk::ImageView view) {
				vk::DescriptorImageInfo imageInfo{
					.imageView = view,
					.imageLayout = vk::ImageLayout::eGeneral
				};

				vk::DescriptorGetInfoEXT getInfo {
					.type = vk::DescriptorType::eSampledImage,
					.data = vk::DescriptorDataEXT(&imageInfo)
				};

				uint64_t descriptorSize = Get().m_PDDBP.sampledImageDescriptorSize;

				std::vector<Byte> payload(descriptorSize);

				VulkanEngine::GetLogicalDevice().getDescriptorEXT(&getInfo, descriptorSize, payload.data());

				AmazingBuffer::UpdateData updateData {
					.offset = Get().m_SampledImageBindingMemOffset + slot * descriptorSize,
					.alignment = descriptorSize,
					.data = std::move(payload)
				};

				auto& db = VulkanUploadManager::GetAmazingBuffer(Get().m_DescriptorBufferHandle);

				db.SubmitUpdate(std::move(updateData));
			}

			static void BindDescriptorBuffer(vk::CommandBuffer& cmb) {
				auto& descriptorBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_DescriptorBufferHandle);

				vk::DescriptorBufferBindingInfoEXT bindInfo {
					.address = descriptorBuffer.GetCurrentFrameLocalBuffer().GetBDA(),
					.usage = vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT | vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT
				};

				cmb.bindDescriptorBuffersEXT(bindInfo);

				uint32_t bufferIndex = 0;
				vk::DeviceSize setOffset = 0;

				cmb.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, Get().m_PipelineLayout, 0, 1, &bufferIndex, &setOffset);
			}


			~VulkanGlobalLayoutManager() {
				VulkanUploadManager::DestroyAmazingBuffer(m_DescriptorBufferHandle);

				VulkanEngine::GetLogicalDevice().destroyPipelineLayout(m_PipelineLayout);
				VulkanEngine::GetLogicalDevice().destroyDescriptorSetLayout(m_DescriptorSetLayout);
			}

		private:
			VulkanGlobalLayoutManager() {
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

				uint64_t alignedSize = AlignUp(setSize, m_PDDBP.descriptorBufferOffsetAlignment);

				m_SamplerBindingMemOffset = VulkanEngine::GetLogicalDevice().getDescriptorSetLayoutBindingOffsetEXT(m_DescriptorSetLayout, 0);
				m_SampledImageBindingMemOffset = VulkanEngine::GetLogicalDevice().getDescriptorSetLayoutBindingOffsetEXT(m_DescriptorSetLayout, 1);

				AmazingBuffer::CreateInfo dbInfo {
					.size = alignedSize,
					.usage = vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT | vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT | vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.queueFamilyIndices = { VulkanEngine::GetGraphicsQueueFamilyIndex() },
					.name = "Global descriptor buffer"
				};

				m_DescriptorBufferHandle = VulkanUploadManager::CreateAmazingBuffer(dbInfo);
			}

			vk::DescriptorSetLayout m_DescriptorSetLayout;
			vk::PipelineLayout m_PipelineLayout;
			vk::PushConstantRange m_PushConstantRange;
			vk::PhysicalDeviceDescriptorBufferPropertiesEXT m_PDDBP;
			vk::DeviceSize m_SamplerBindingMemOffset{ 0 };
			vk::DeviceSize m_SampledImageBindingMemOffset{ 0 };

			static constexpr uint16_t s_MaxTextures{ 16 * 1024 };
			static constexpr uint16_t s_MaxSamplers{ 1 * 1024 };

			AmazingBufferHandle m_DescriptorBufferHandle{ 0 };

			static std::unique_ptr<VulkanGlobalLayoutManager> s_Instance;
		};
	}
}
