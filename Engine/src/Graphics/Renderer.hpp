#pragma once
#include "Vulkan/VulkanEngine.hpp"
#include "Vulkan/VulkanResourceTracker.hpp"
#include "RenderGraph.hpp"
#include "Core/Time.hpp"
#include "Vulkan/VulkanUploadManager.hpp"
#include "Vulkan/VulkanMeshManager.hpp"
#include "Vulkan/VulkanShaderManager.hpp"
#include "Vulkan/VulkanLayoutManager.hpp"
#include "Vulkan/VulkanTextureManager.hpp"
#include "FileSystem/PathManager.hpp"
#include "Image.hpp"

//FIXME: need explicit Renderer lifetime control, its deleted after VulkanEngine has been shutdown

namespace Cori {
	namespace Graphics {
		const ResourceState StateA = { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal };
		const ResourceState StateB = { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral };
		const ResourceState StateC = { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eColorAttachmentOptimal };
		const ResourceState StateD = { vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::ImageLayout::eDepthAttachmentOptimal };

		class Renderer {
		public:
			Renderer() {
				std::ifstream file(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/shader.spv", std::ios::ate | std::ios::binary);
				if (!file.is_open()) {
					throw std::runtime_error("failed to open file!");
				}

				std::vector<Byte> buffer(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
				file.close();


				shader = VulkanShaderManager::CreateVertexShaderPair(buffer.data(), buffer.size(), "vertMain", "fragMain", "Test Shader");

				float depth = 0.0f;

				std::vector<Vertex> vertices = {
					// Bottom-left
					{ glm::vec3(-0.5f, -0.5f, depth),   0.0f, glm::vec3(0,0,1),   0.0f, glm::vec4(1,1,1,1) },

					// Bottom-right
					{ glm::vec3( 0.5f, -0.5f, depth),   1.0f, glm::vec3(0,0,1),   0.0f, glm::vec4(1,1,1,1) },

					// Top-right
					{ glm::vec3( 0.5f,  0.5f, depth),   1.0f, glm::vec3(0,0,1),   1.0f, glm::vec4(1,1,1,1) },

					// Top-left
					{ glm::vec3(-0.5f,  0.5f, depth),   0.0f, glm::vec3(0,0,1),   1.0f, glm::vec4(1,1,1,1) }
				};

				std::vector<uint32_t> indices = {
					0, 1, 2,   // first triangle
					2, 3, 0    // second triangle
				};

				quad = VulkanMeshManager::CreateMesh(vertices.data(), vertices.size() * sizeof(Vertex), indices.data(), indices.size() * sizeof(uint32_t));

				auto image = Image::Create(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "placeholders/uv_sample.png");

				texture = VulkanTextureManager::CreateTextureTest(image->GetPixelData(), image->GetHeight() * image->GetWidth() * 4, vk::Format::eR8G8B8A8Srgb, { image->GetHeight(), image->GetWidth() }, "Test Texture");
			}

			~Renderer() {
				buffer.Destroy();
				image.Destroy();
			}

			static Renderer& Get() {
				static Renderer instance;
				return instance;
			}

			static void Render() {
				Get();
				CORI_PROFILE_FUNCTION();
				auto& frameData = VulkanEngine::Get().BeginFrame();
				if (!frameData.m_SkippedFrame) {
					vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f);
					vk::RenderingAttachmentInfo attachmentInfo = {
						.imageView = VulkanEngine::GetSwapChainImageView(),
						.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
						.loadOp = vk::AttachmentLoadOp::eClear,
						.storeOp = vk::AttachmentStoreOp::eStore,
						.clearValue = clearColor
					};

					vk::RenderingInfo renderingInfo = {
						.renderArea = {.offset = {0, 0}, .extent = VulkanEngine::GetSwapChainExtent()},
						.layerCount = 1,
						.colorAttachmentCount = 1,
						.pColorAttachments = &attachmentInfo
					};

					frameData.m_CommandBuffer.beginRendering(renderingInfo);

					VulkanGlobalLayoutManager::BindDescriptorBuffer(frameData.m_CommandBuffer);
					VulkanShaderManager::GetShaderObject(Get().shader).Bind(frameData.m_CommandBuffer);

					frameData.m_CommandBuffer.bindIndexBuffer(VulkanMeshManager::GetIndexBuffer().m_Buffer, 0, vk::IndexType::eUint32);

					struct PushConstants {
						uint64_t vertexBufferAddress{ 0 };
						uint64_t meshAssetDataAddress{ 0 };
					} pc;

					pc.vertexBufferAddress = VulkanMeshManager::GetVertexSSBO().GetBDA();
					pc.meshAssetDataAddress = VulkanMeshManager::GetFrameLocal().GetBDA();

					frameData.m_CommandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(pc), &pc);

					frameData.m_CommandBuffer.drawIndexed(6, 1, 0, 0, 0);

					frameData.m_CommandBuffer.endRendering();
				}

				VulkanEngine::Get().EndFrame();

				//VulkanResourceTracker* tracker = &VulkanResourceTracker::Get();

				//VulkanUploadManager::Get();

				#if 0
				VulkanUploadManager* manager = &VulkanUploadManager::Get();

				auto res1 = manager->Allocate(1024, 4);
				auto res2 = manager->Allocate(1024, 4);
				manager->SetFence(nullptr);
				manager->test = true;
				auto res3 = manager->Allocate(1024, 4);
				manager->test = false;

				auto res4 = manager->Allocate(512, 4);

				manager->SetFence(nullptr);


				auto res5 = manager->Allocate(1024, 4);


				auto res6 = manager->Allocate(1024, 4);

				auto res7 = manager->Allocate(1024, 4);

				manager->test = true;

				auto res8 = manager->Allocate(1024, 4);

				manager->test = false;

				auto res9 = manager->Allocate(1024, 4);
				auto res10 = manager->Allocate(1024, 4);

				manager->SetFence(nullptr);

				manager->test = true;

				//?????
				auto res11 = manager->Allocate(1024, 4);

				manager->test = false;

				#endif

				#if 0

				ResourceState stateA{
					.stageMask = vk::PipelineStageFlagBits2::eVertexInput,
					.accessMask = vk::AccessFlagBits2::eShaderRead,
				};

				auto opt = VulkanResourceTracker::TransitionBuffer(Get().buffer, 0, VK_WHOLE_SIZE, stateA);

				std::vector<vk::BufferMemoryBarrier2> wholebar = *opt.value();

				ResourceState stateB{
					.stageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.accessMask = vk::AccessFlagBits2::eShaderWrite,
				};

				auto opt1 = VulkanResourceTracker::TransitionBuffer(Get().buffer, 1024, 1024, stateB);

				std::vector<vk::BufferMemoryBarrier2> quaterbar = *opt1.value();

				ResourceState octa{
					.stageMask = vk::PipelineStageFlagBits2::eFragmentShader,
					.accessMask = vk::AccessFlagBits2::eShaderSampledRead,
				};

				auto opt2 = VulkanResourceTracker::TransitionBuffer(Get().buffer, 2048, 1024, stateB);

				std::vector<vk::BufferMemoryBarrier2> octabar = *opt2.value();

				ResourceState half{
					.stageMask = vk::PipelineStageFlagBits2::eTransfer,
					.accessMask = vk::AccessFlagBits2::eTransferWrite,
				};

				auto opt3 = VulkanResourceTracker::TransitionBuffer(Get().buffer, 0, VK_WHOLE_SIZE, half);

				std::vector<vk::BufferMemoryBarrier2> halfbar = *opt3.value();

				//VulkanResourceTracker::UnregisterBuffer(Get().buffer);
				//VulkanResourceTracker::RegisterBuffer(Get().buffer);
				#else

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 0, 10, 0, 1 }, StateA);

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 3, 4, 0, 1 }, StateB);

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 2, 7, 0, 1 }, StateC);

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 3, 2, 0, 1 }, StateB);

				//VulkanResourceTracker::UnregisterImage(Get().image);
				//VulkanResourceTracker::RegisterImage(Get().image);

				#endif
			}
		private:
			ShaderObjectHandle shader;
			MeshHandle quad;
			VulkanBuffer buffer;
			VulkanImage image;
			TextureHandle texture;
			RenderGraphResourceRegistry m_GraphResourceRegistry;
			RenderGraphPassRegistry m_GraphPassRegistry;
		};
	}
}