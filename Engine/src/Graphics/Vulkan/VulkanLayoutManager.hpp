#pragma once
#include "VulkanEngine.hpp"
#include "VulkanUploadSubsystem.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanGlobalLayoutManager {
		public:
			static void Init();

			static void Shutdown();

			static VulkanGlobalLayoutManager& Get();

			[[nodiscard]] static vk::DescriptorSetLayout& GetGlobalDescriptorSetLayout() {
				return Get().m_DescriptorSetLayout;
			}

			[[nodiscard]] static vk::PipelineLayout& GetGlobalPipelineLayout() {
				return Get().m_PipelineLayout;
			}

			[[nodiscard]] static vk::PushConstantRange& GetGlobalPushConstantRange() {
				return Get().m_PushConstantRange;
			}

			[[nodiscard]] static uint16_t GetMaxTextures() {
				return s_MaxTextures;
			}

			[[nodiscard]] static uint16_t GetMaxSamplers() {
				return s_MaxSamplers;
			}

			static void Sync() {
				Get().m_DescriptorBuffer.Sync();
			}

			static void UpdateSamplerDescriptor(const uint32_t slot, const vk::Sampler sampler);

			static void UpdateSampledTextureDescriptor(const uint32_t slot, const vk::ImageView view);

			static void BindDescriptorBuffer(vk::CommandBuffer& cmb);

			~VulkanGlobalLayoutManager();

		private:
			VulkanGlobalLayoutManager();

			vk::DescriptorSetLayout m_DescriptorSetLayout;
			vk::PipelineLayout m_PipelineLayout;
			vk::PushConstantRange m_PushConstantRange;
			vk::PhysicalDeviceDescriptorBufferPropertiesEXT m_PDDBP;
			vk::DeviceSize m_SamplerBindingMemOffset{ 0 };
			vk::DeviceSize m_SampledImageBindingMemOffset{ 0 };

			static constexpr uint16_t s_MaxTextures{ 1 * 1024 };
			static constexpr uint16_t s_MaxSamplers{ 1 * 1024 };

			VulkanDynamicVector<Byte> m_DescriptorBuffer{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT | vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT | vk::BufferUsageFlagBits::eShaderDeviceAddress, "Global Descriptor Buffer" };

			static std::unique_ptr<VulkanGlobalLayoutManager> s_Instance;
		};
	}
}
