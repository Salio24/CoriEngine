#pragma once
#include "VulkanEngine.hpp"
#include "VulkanImage.hpp"
#include "VulkanLayoutManager.hpp"
#include <entt/container/dense_set.hpp>

namespace Cori {
	namespace Graphics {
		using TextureHandle = uint32_t;
		using SamplerHandle = uint32_t;

		class VulkanTextureManager {
		public:
			static void Init();

			static void Shutdown();

			static VulkanTextureManager& Get();

			static TextureHandle CreateTextureTest(void* pixels, uint64_t pixelDataSize, const vk::Format format, const vk::Extent2D extent, const char* name = "") {
				std::vector<Byte> pixelData(pixelDataSize);

				memcpy(pixelData.data(), pixels, pixelDataSize);

				return CreateTextureTest(std::move(pixelData), format, extent, name);
			}

			static void TransitionLoadedImages() {

			}

			static VulkanImage& GetTexture(uint32_t handle) {
				return Get().m_TexturePool[handle].image;
			}

			static void UpdateTexture(const TextureHandle handle, std::vector<Byte>&& pixels, const vk::Offset3D& offset, const vk::Extent3D& extent, vk::ImageSubresourceLayers subresourceLayers) {
				CORI_CORE_ASSERT(extent.width > 0 && extent.height > 0 && extent.depth > 0, "Invalid texture extent passed to VulkanTextureManager::UpdateTexture." );
				CORI_CORE_ASSERT(handle < VulkanGlobalLayoutManager::GetMaxTextures() - 1, "Invalid TextureHandle was passed to VulkanTextureManager::ChangeView.");

				auto& texture = Get().m_TexturePool[handle];
				if (!texture.valid) {
					//TODO: warn
					return;
				}

				CORI_CORE_ASSERT(pixels.size() == vk::blockSize(texture.image.m_Format) * extent.width * extent.height, "Data size provided to VulkanTextureManager::CreateTexture doesn't match the expected size considering format an extent.");

				VulkanUploadManager::ImageUploadRange uploadRange{
					.offset = offset,
					.extent = extent,
					.subresourceLayers = subresourceLayers
				};

				VulkanUploadManager::UploadRequest uploadRequest{
					.uploadParts = VulkanUploadManager::UploadPart{ texture.image, uploadRange, std::move(pixels) },
					.callback = VulkanTextureManager::UpdateLoadedTextures,
					.uploadType = VulkanUploadManager::UploadType::Streaming,
					.userData = reinterpret_cast<void*>(static_cast<uint64_t>(handle))
				};

				VulkanUploadManager::SubmitUploadRequest(std::move(uploadRequest));
			}

			static void ChangeView(const TextureHandle handle, vk::ImageViewType viewType, vk::ImageSubresourceRange subresourceRange) {
				CORI_CORE_ASSERT(handle < VulkanGlobalLayoutManager::GetMaxTextures() - 1, "Invalid TextureHandle was passed to VulkanTextureManager::ChangeView.");

				auto& texture = Get().m_TexturePool[handle];
				if (!texture.valid) {
					//TODO: warn
					return;
				}

				VulkanImage::ImageViewKey viewKey{
					.type = viewType,
					.subresourceRange = subresourceRange
				};

				texture.view = texture.image.GetView(viewKey);
			}

			static void UpdateView(const TextureHandle handle) {
				CORI_CORE_ASSERT(handle < VulkanGlobalLayoutManager::GetMaxTextures() - 1, "Invalid TextureHandle was passed to VulkanTextureManager::ChangeView.");

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(handle, Get().m_TexturePool[handle].view);
			}

			static TextureHandle CreateTexture(const vk::ImageType type, const vk::Format format, const vk::Extent3D& extent, const uint32_t mipCount, const uint32_t layerCount, const vk::SampleCountFlagBits sampleFlags, const char* name = "") {
				CORI_CORE_ASSERT(extent.width > 0 && extent.height > 0 && extent.depth > 0, "Invalid texture extent passed to VulkanTextureManager::CreateTexture." );

				TextureHandle freeHandle;
				if (!Get().m_Holes.empty()) {
					freeHandle = Get().m_Holes.back();
					Get().m_Holes.pop_back();
				} else {
					freeHandle = Get().m_NextTextureHandle++;
					CORI_CORE_ASSERT(freeHandle < VulkanGlobalLayoutManager::GetMaxTextures() - 1, "VulkanTextureManager out of texture slots.");
				}

				auto& texture = Get().m_TexturePool[freeHandle];

				vk::ImageCreateInfo imageCreateInfo {
					.imageType = type,
					.format = format,
					.extent = extent,
					.mipLevels = mipCount,
					.arrayLayers = layerCount,
					.samples = sampleFlags,
					.tiling = vk::ImageTiling::eOptimal,
					.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					.sharingMode = Get().m_TextureSharingMode,
					.queueFamilyIndexCount = static_cast<uint32_t>(Get().m_QueueFamilyIndices.size()),
					.pQueueFamilyIndices = Get().m_QueueFamilyIndices.data(),
					.initialLayout = vk::ImageLayout::eUndefined
				};

				vma::AllocationCreateInfo allocInfo {
					.usage = vma::MemoryUsage::eAuto
				};

				bool named = strcmp(name, "") != 0;

				VulkanImage::CreateInfo info {
					.imageCreateInfo = &imageCreateInfo,
					.allocationCreateInfo = &allocInfo,
				};

				if (named) {
					info.name = name;
				}

				texture.image = VulkanImage::Create(info);
				texture.valid = true;
				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(freeHandle, Get().m_TexturePool[0].view);
				return freeHandle;
			}

			static TextureHandle CreateTextureTest(std::vector<Byte>&& pixels, const vk::Format format, const vk::Extent2D extent, const char* name = "") {
				auto handle = CreateTexture(vk::ImageType::e2D, format, { extent.width, extent.height, 1 }, 1, 1, vk::SampleCountFlagBits::e1, name);
				UpdateTexture(handle, std::move(pixels), { 0, 0, 0 }, { extent.width, extent.height, 1 }, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 });
				ChangeView(handle, vk::ImageViewType::e2D, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				return handle;
			}

			static void DestroyTexture(TextureHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_TexturePool.size(), "Invalid TextureHandle was passed to VulkanTextureManager::DestroyTexture.");
				auto& texture = Get().m_TexturePool[handle];
				if (!texture.valid) {
					return;
				}

				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				uint32_t prevFrame = (frameIndex + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT;
				Get().m_DestructionQueue[prevFrame].emplace_back(handle);
				texture.valid = false;
			}

			static void ProcessDestructionQueue() {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				for (auto handle : Get().m_DestructionQueue[frameIndex]) {
					VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(handle, Get().m_TexturePool[1].view);

					Get().m_Holes.emplace_back(handle);
				}
			}

			static void UpdateLoadedTextures(void* data) {
				TextureHandle handle = static_cast<TextureHandle>(reinterpret_cast<uint64_t>(data));

				UpdateView(handle);
			}

			~VulkanTextureManager() {
				for (auto& sampler : m_SamplerPool) {
					VulkanEngine::GetLogicalDevice().destroySampler(sampler);
				}

				for (auto& texture : m_TexturePool) {
					if (texture.image.m_Image) {
						texture.image.Destroy();
					}
				}
			}

		private:
			VulkanTextureManager() {
				uint32_t transferQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex();

				std::vector<uint32_t> queueFamilyIndices{ VulkanEngine::GetGraphicsQueueFamilyIndex() };

				bool transferQueueInVector = false;
				for (auto familyIndex : queueFamilyIndices) {
					if (familyIndex == transferQueueFamilyIndex) {
						transferQueueInVector = true;
					}
				}

				if (transferQueueInVector && queueFamilyIndices.size() == 1) {
					m_TextureSharingMode = vk::SharingMode::eExclusive;
				} else if (transferQueueInVector && queueFamilyIndices.size() != 1) {
					m_TextureSharingMode = vk::SharingMode::eConcurrent;
					m_QueueFamilyIndices = queueFamilyIndices;
				} else {
					queueFamilyIndices.emplace_back(transferQueueFamilyIndex);
					m_TextureSharingMode = vk::SharingMode::eConcurrent;
					m_QueueFamilyIndices = queueFamilyIndices;
				}

				vk::ImageCreateInfo imageCreateInfo {
					.imageType = vk::ImageType::e2D,
					.format = vk::Format::eR8G8B8A8Srgb,
					.extent = vk::Extent3D{ 8, 8, 1 },
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = vk::SampleCountFlagBits::e1,
					.tiling = vk::ImageTiling::eOptimal,
					.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					.sharingMode = m_TextureSharingMode,
					.queueFamilyIndexCount = static_cast<uint32_t>(m_QueueFamilyIndices.size()),
					.pQueueFamilyIndices = m_QueueFamilyIndices.data(),
					.initialLayout = vk::ImageLayout::eUndefined
				};

				vma::AllocationCreateInfo allocInfo {
					.usage = vma::MemoryUsage::eAuto
				};

				VulkanImage::CreateInfo whiteTextureCreateInfo {
					.imageCreateInfo = &imageCreateInfo,
					.allocationCreateInfo = &allocInfo,
					.name = "White placeholder texture"
				};

				VulkanImage::CreateInfo missingTextureCreateInfo {
					.imageCreateInfo = &imageCreateInfo,
					.allocationCreateInfo = &allocInfo,
					.name = "Missing texture placeholder texture"
				};

				m_TexturePool.resize(VulkanGlobalLayoutManager::GetMaxTextures());
				m_SamplerPool.resize(VulkanGlobalLayoutManager::GetMaxSamplers());
				for (auto& queue : m_DestructionQueue) {
					queue.reserve(64);
				}

				auto& whiteTexture = m_TexturePool[0];
				auto& missingTexturePlaceholder = m_TexturePool[1];

				whiteTexture.image = VulkanImage::Create(whiteTextureCreateInfo);
				missingTexturePlaceholder.image = VulkanImage::Create(missingTextureCreateInfo);

				VulkanImage::ImageViewKey viewKey{
					.type = vk::ImageViewType::e2D,
					.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
				};

				whiteTexture.view = whiteTexture.image.GetView(viewKey);
				missingTexturePlaceholder.view = missingTexturePlaceholder.image.GetView(viewKey);

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(0, whiteTexture.view);
				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(1, missingTexturePlaceholder.view);

				std::vector<Byte> whiteData(256, 0xFF);
				std::vector<Byte> missingData(256);

				for (uint32_t i = 0; i < 64; i++) {
					uint32_t x =  i % 8;
					uint32_t y =  floor(i / 8);

					bool top  = y < 4;
					bool left = x < 4;

					bool isBlack = (top && left) || (!top && !left);

					missingData[i * 4] = isBlack ? 0x00 : 0x0F;
					missingData[i * 4 + 1] = 0x00;
					missingData[i * 4 + 2] = isBlack ? 0x00 : 0x0F;
					missingData[i * 4 + 3] = 0xFF;
				}

				VulkanUploadManager::ImageUploadRange uploadRange{
					.offset = { 0, 0, 0 },
					.extent = { 8, 8, 1 },
					.subresourceLayers = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }
				};

				VulkanUploadManager::UploadRequest whiteRequest{
					.uploadParts = VulkanUploadManager::UploadPart{ whiteTexture.image, uploadRange, std::move(whiteData) },
					.uploadType = VulkanUploadManager::UploadType::FrameCritical
				};

				VulkanUploadManager::UploadRequest missingRequest{
					.uploadParts = VulkanUploadManager::UploadPart{ missingTexturePlaceholder.image, uploadRange, std::move(missingData) },
					.uploadType = VulkanUploadManager::UploadType::FrameCritical
				};

				VulkanUploadManager::SubmitUploadRequest(std::move(whiteRequest));
				VulkanUploadManager::SubmitUploadRequest(std::move(missingRequest));

				for (uint32_t i = 2; i < VulkanGlobalLayoutManager::GetMaxTextures(); i++) {
					VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(i, m_TexturePool[1].view);
				}

				vk::SamplerCreateInfo samplerInfo{
					.magFilter = vk::Filter::eNearest,
					.minFilter = vk::Filter::eNearest,
					.mipmapMode = vk::SamplerMipmapMode::eNearest,
					.addressModeU = vk::SamplerAddressMode::eClampToEdge,
					.addressModeV = vk::SamplerAddressMode::eClampToEdge,
					.addressModeW = vk::SamplerAddressMode::eClampToEdge,
					.anisotropyEnable = vk::False
				};

				auto [result, sampler] = VulkanEngine::GetLogicalDevice().createSampler(samplerInfo);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create sampler. Error: {}", vk::to_string(result));

				VulkanGlobalLayoutManager::UpdateSamplerDescriptor(0, sampler);
				m_SamplerPool[0] = sampler;
			}

			struct Texture {
				VulkanImage image;
				vk::ImageView view;
				bool valid{ false };
			};


			//std::vector<std::pair<TextureHandle, VulkanUploadManager::ImageUploadRange>> m_PendingUploads;
			//std::vector<uint32_t> m_UploadVectorHoles;

			std::vector<Texture> m_TexturePool;
			std::vector<TextureHandle> m_Holes;
			std::vector<vk::Sampler> m_SamplerPool;

			vk::SharingMode m_TextureSharingMode;
			std::vector<uint32_t> m_QueueFamilyIndices;

			TextureHandle m_NextTextureHandle{ 2 };
			SamplerHandle m_NextSamplerHandle{ 1 };

			std::array<std::vector<TextureHandle>, FRAMES_IN_FLIGHT> m_DestructionQueue;


			static std::unique_ptr<VulkanTextureManager> s_Instance;

		};
	}
}
