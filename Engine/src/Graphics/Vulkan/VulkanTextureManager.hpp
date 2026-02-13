#pragma once
#include "VulkanEngine.hpp"
#include "VulkanImage.hpp"
#include "VulkanLayoutManager.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"

namespace Cori {
	namespace Graphics {
		using SamplerHandle = uint32_t;

		struct Texture {

		protected:
			friend class VulkanTextureManager;
			VulkanImage image;
			vk::ImageView view;
			uint32_t descriptorIndex{ 0 };
			bool acquiredByGraphics{ false };
		};

		class VulkanTextureManager {
			struct QueuedUpload {
				VulkanStreamingLine::ImageUpload imageUpload;
				std::vector<Byte> data;
				Core::Handle<Texture> texture;
			};
		public:
			static void Init();

			static void Shutdown();

			static VulkanTextureManager& Get();

			[[nodiscard]] static SamplerHandle GetSampler(const vk::SamplerCreateInfo& info) {
				auto [it, result] = Get().m_SamplerMap.try_emplace(info);
				if (!result) {
					return it->second;
				}

				uint32_t slot = Get().m_Samplers.size();

				auto [result_, sampler] = VulkanEngine::GetLogicalDevice().createSampler(info);
				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create sampler. Error: {}", vk::to_string(result_));

				VulkanGlobalLayoutManager::UpdateSamplerDescriptor(slot, sampler);
				Get().m_Samplers[slot] = sampler;

				it->second = slot;
				return slot;
			}

			[[nodiscard]] static Core::Handle<Texture> GetPlaceholderTexture() {
				return Get().m_PlaceholderTexture;
			}

			[[nodiscard]] static Core::Handle<Texture> GetWhiteTexture() {
				return Get().m_PlaceholderTexture;
			}

			[[nodiscard]] static VulkanImage& GetTexture(uint32_t handle) {
				return Get().m_TexturePool[handle].image;
			}

			[[nodiscard]] static uint64_t GetTextureAssetTableBDA() {
				return Get().m_GPUAssetTables.GetVulkanBuffer().GetBDA();
			}

			static void UpdateTexture(const Core::Handle<Texture> handle, std::vector<Byte>&& pixels, const vk::Offset3D& offset, const vk::Extent3D& extent, const vk::ImageSubresourceLayers& subresourceLayers) {
				if (!(extent.width > 0 && extent.height > 0 && extent.depth > 0)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Invalid texture extent passed to UpdateTexture, aborting.");
					return;
				}

				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to ChangeView is invalid, aborting.");
					return;
				}

				auto& texture = Get().m_TexturePool[handle];

				texture.acquiredByGraphics = true;

				VulkanStreamingLine::ImageUpload resUpload{
					.resource = texture.image,
					.range = { .offset = offset, .extent = extent, .subresourceLayers = subresourceLayers },
					.dstLayout = s_DstLayout,
					.dstQueueFamilyIndex = VulkanEngine::GetGraphicsQueueFamilyIndex()
				};

				VulkanStreamingLine::GenericUpload request{
					.resourceUpload = resUpload,
					.data = pixels
				};

				auto result = VulkanStreamingLine::SubmitUploads(request);

				if (result) {
					Get().FindInTransferSlot(result.value()).emplace_back(handle, subresourceLayers);
				} else {
					if (result.error() == ErrorCode::eInvalidData) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "VulkanStreamingLine returned with error code eInvalidData during UpdateTexture call, aborting.");
						return;
					}

					Get().m_QueuedUploads.emplace(resUpload, std::move(pixels), handle);
				}
			}

			static void ChangeView(const Core::Handle<Texture> handle, const vk::ImageViewType viewType, const vk::ImageSubresourceRange& subresourceRange) {
				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to ChangeView is invalid, aborting.");
					return;
				}

				auto& texture = Get().m_TexturePool[handle];

				VulkanImage::ImageViewKey viewKey{
					.type = viewType,
					.subresourceRange = subresourceRange
				};

				texture.view = texture.image.GetView(viewKey);
			}

			static void UpdateView(const Core::Handle<Texture> handle) {
				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to UpdateView is invalid, aborting.");
					return;
				}

				auto& texture = Get().m_TexturePool[handle];

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, texture.view);
			}

			static void ProcessUpdates(vk::CommandBuffer cmb) {
				Get().m_BarrierCache.clear();
				auto currentTimelineValue = VulkanStreamingLine::GetTimelineValue();

				for (auto& [ticket, handles] : Get().m_TexturesInTransfer) {
					if (currentTimelineValue >= ticket) {
						for (auto& [handle, subresource] : handles) {
							UpdateView(handle);

							vk::ImageSubresourceRange vkRange{
								.aspectMask = subresource.aspectMask,
								.baseMipLevel = subresource.mipLevel,
								.levelCount = 1,
								.baseArrayLayer = subresource.baseArrayLayer,
								.layerCount = subresource.layerCount
							};

							auto& texture = Get().m_TexturePool[handle];

							Get().m_BarrierCache.emplace_back(vk::ImageMemoryBarrier2{
								.srcStageMask = vk::PipelineStageFlagBits2::eNone,
								.srcAccessMask = vk::AccessFlagBits2::eNone,
								.dstStageMask = vk::PipelineStageFlagBits2::eNone,
								.dstAccessMask = vk::AccessFlagBits2::eNone,
								.oldLayout = s_DstLayout,
								.newLayout = s_DstLayout,
								.srcQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex(),
								.dstQueueFamilyIndex = VulkanEngine::GetGraphicsQueueFamilyIndex(),
								.image = texture.image.m_Image,
								.subresourceRange = vkRange
							});
						}

						handles.clear();
					}
				}

				if (!Get().m_BarrierCache.empty()) {
					vk::DependencyInfo depInfo{
						.imageMemoryBarrierCount = static_cast<uint32_t>(Get().m_BarrierCache.size()),
						.pImageMemoryBarriers = Get().m_BarrierCache.data()
					};

					cmb.pipelineBarrier2(depInfo);
				}

				while (!Get().m_QueuedUploads.empty()) {
					auto& upload = Get().m_QueuedUploads.front();

					VulkanStreamingLine::GenericUpload request{
						.resourceUpload = upload.imageUpload,
						.data = upload.data
					};

					auto result = VulkanStreamingLine::SubmitUploads(request);

					if (result) {
						Get().FindInTransferSlot(result.value()).emplace_back(upload.texture, upload.imageUpload.range.subresourceLayers);
						Get().m_QueuedUploads.pop();
					} else {
						break;
					}
				}
			}

			[[nodiscard]] static Core::Handle<Texture> CreateTexture(const vk::ImageType type, const vk::Format format, const vk::Extent3D& extent, const uint32_t mipCount, const uint32_t layerCount, const vk::SampleCountFlagBits sampleFlags, const char* name = "") {
				CORI_CORE_ASSERT(extent.width > 0 && extent.height > 0 && extent.depth > 0, "Invalid texture extent passed to VulkanTextureManager::CreateTexture." );

				vk::ImageCreateInfo imageCreateInfo {
					.imageType = type,
					.format = format,
					.extent = extent,
					.mipLevels = mipCount,
					.arrayLayers = layerCount,
					.samples = sampleFlags,
					.tiling = vk::ImageTiling::eOptimal,
					.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					.sharingMode = vk::SharingMode::eExclusive,
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

				Texture texture;
				texture.image = VulkanImage::Create(info);
				texture.descriptorIndex = Get().m_FreeTextureDescriptorSlots.back();
				Get().m_FreeTextureDescriptorSlots.pop_back();

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, Get().m_TexturePool[Get().m_WhiteTexture].view);

				auto handle = Get().m_TexturePool.Emplace(texture);
				if (handle.GetIndex() >= Get().m_GPUAssetTables.Size()) {
					Get().m_GPUAssetTables.Resize(Get().m_GPUAssetTables.Size() * 1.5f);
				}

				auto& table = Get().m_GPUAssetTables[handle.GetIndex()].get();
				table.descriptorIndex = texture.descriptorIndex;
				table.version = handle.GetVersion();

				return handle;
			}

			static void DestroyTexture(const Core::Handle<Texture> handle) {
				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to DestroyTexture is invalid, aborting.");
					return;
				}

				if (handle == Get().m_PlaceholderTexture || handle == Get().m_WhiteTexture) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to DestroyTexture is a placeholder texture, you can't delete those, aborting.");
					return;
				}

				auto& texture = Get().m_TexturePool[handle];

				DeletionQueue::PushImage(texture.image, VulkanEngine::GetCurrentFrameInFlight());

				auto& table = Get().m_GPUAssetTables[handle.GetIndex()].get();
				table = {};

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, Get().m_TexturePool[Get().m_PlaceholderTexture].view);

				Get().m_FreeTextureDescriptorSlots.emplace_back(texture.descriptorIndex);

				Get().m_TexturePool.Remove(handle);
			}

			~VulkanTextureManager() {
				for (auto& texture : m_TexturePool) {
					if (texture.image.m_Image) {
						DeletionQueue::PushImage(texture.image, VulkanEngine::GetCurrentFrameInFlight());
					}
				}
			}

		private:
			VulkanTextureManager() {
				vk::ImageCreateInfo imageCreateInfo {
					.imageType = vk::ImageType::e2D,
					.format = vk::Format::eR8G8B8A8Srgb,
					.extent = vk::Extent3D{ 8, 8, 1 },
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = vk::SampleCountFlagBits::e1,
					.tiling = vk::ImageTiling::eOptimal,
					.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					.sharingMode = vk::SharingMode::eExclusive,
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

				m_TexturePool.Reserve(VulkanGlobalLayoutManager::GetMaxTextures());
				m_Samplers.reserve(VulkanGlobalLayoutManager::GetMaxSamplers());

				m_GPUAssetTables.Resize(VulkanGlobalLayoutManager::GetMaxTextures());
				m_FreeTextureDescriptorSlots.reserve(VulkanGlobalLayoutManager::GetMaxTextures() - 2);

				uint32_t counter = VulkanGlobalLayoutManager::GetMaxTextures() - 1;
				for (auto& index : m_FreeTextureDescriptorSlots) {
					index = counter--;
				}

				Texture missingTexturePlaceholder;
				Texture whiteTexture;

				missingTexturePlaceholder.image = VulkanImage::Create(missingTextureCreateInfo);
				whiteTexture.image = VulkanImage::Create(whiteTextureCreateInfo);

				VulkanImage::ImageViewKey viewKey{
					.type = vk::ImageViewType::e2D,
					.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
				};

				missingTexturePlaceholder.view = missingTexturePlaceholder.image.GetView(viewKey);
				whiteTexture.view = whiteTexture.image.GetView(viewKey);

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(s_PlaceholderTextureDescriptorIndex, missingTexturePlaceholder.view);
				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(s_WhiteTextureDescriptorIndex, whiteTexture.view);

				std::vector<Byte> missingData(256);
				std::vector<Byte> whiteData(256, 0xFF);

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

				VulkanStreamingLine::ImageUploadRange uploadRange{
					.offset = { 0, 0, 0 },
					.extent = { 8, 8, 1 },
					.subresourceLayers = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }
				};

				std::array<VulkanStreamingLine::GenericUpload, 2> uploads;

				uploads[0] = {
					.resourceUpload = VulkanStreamingLine::ImageUpload{ whiteTexture.image, uploadRange, vk::ImageLayout::eShaderReadOnlyOptimal, VulkanEngine::GetGraphicsQueueFamilyIndex() },
					.data = whiteData
				};

				uploads[1] = {
					.resourceUpload = VulkanStreamingLine::ImageUpload{ missingTexturePlaceholder.image, uploadRange, vk::ImageLayout::eShaderReadOnlyOptimal, VulkanEngine::GetGraphicsQueueFamilyIndex() },
					.data = missingData
				};

				auto ticket = VulkanStreamingLine::SubmitUploads(uploads);

				m_PlaceholderTexture = m_TexturePool.Emplace(missingTexturePlaceholder);
				m_WhiteTexture = m_TexturePool.Emplace(whiteTexture);

				auto& table = Get().m_GPUAssetTables[m_PlaceholderTexture.GetIndex()].get();
				table.descriptorIndex = s_PlaceholderTextureDescriptorIndex;
				table.version = m_PlaceholderTexture.GetVersion();

				auto& table_ = Get().m_GPUAssetTables[m_PlaceholderTexture.GetIndex()].get();
				table_.descriptorIndex = s_WhiteTextureDescriptorIndex;
				table_.version = m_PlaceholderTexture.GetVersion();

				CORI_CORE_ASSERT(ticket, "Failed to submit uploads for white or placeholder texture to streaming line. Error: ", to_string(ticket.error()));

				VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket.value(), vk::PipelineStageFlagBits::eFragmentShader);

				for (uint32_t i = 2; i < VulkanGlobalLayoutManager::GetMaxTextures() - 2; i++) {
					VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(i, m_TexturePool[m_PlaceholderTexture].view);
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
				m_Samplers.push_back(sampler);
				m_SamplerMap.insert({ samplerInfo, 0 });
			}

			std::vector<std::pair<Core::Handle<Texture>, vk::ImageSubresourceLayers>>& FindInTransferSlot(const uint64_t value) {
				std::pair<uint64_t, std::vector<std::pair<Core::Handle<Texture>, vk::ImageSubresourceLayers>>>* free = nullptr;
				std::pair<uint64_t, std::vector<std::pair<Core::Handle<Texture>, vk::ImageSubresourceLayers>>>* bestPick = nullptr;

				for (auto& pair : m_TexturesInTransfer) {
					if (pair.first == value) {
						bestPick = &pair;
						break;
					}

					if (pair.second.empty()) {
						free = &pair;
					}
				}

				if (bestPick) {
					bestPick->first = value;
					return bestPick->second;
				}

				free->first = value;
				return free->second;
			}

			Core::FlatSlotMap<Texture> m_TexturePool;

			std::vector<uint32_t> m_FreeTextureDescriptorSlots;

			struct GPUAssetTable {
				uint32_t descriptorIndex{ 0 };
				uint32_t version{ 0 };
			};

			Core::Handle<Texture> m_PlaceholderTexture;
			Core::Handle<Texture> m_WhiteTexture;

			static constexpr uint32_t s_PlaceholderTextureDescriptorIndex{ 0 };
			static constexpr uint32_t s_WhiteTextureDescriptorIndex{ 1 };

			VulkanDynamicVector<GPUAssetTable> m_GPUAssetTables{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Texture Asset Look Up Table" };

			std::vector<vk::ImageMemoryBarrier2> m_BarrierCache;

			std::array<std::pair<uint64_t, std::vector<std::pair<Core::Handle<Texture>, vk::ImageSubresourceLayers>>>, TRANSFERS_IN_FLIGHT + 1> m_TexturesInTransfer;
			std::queue<QueuedUpload> m_QueuedUploads;

			struct SCIHasher {
				std::size_t operator()(const vk::SamplerCreateInfo& info) const noexcept {
					uint64_t hash;

					Utility::HashCombine(hash, static_cast<uint32_t>(info.flags));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.magFilter));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.minFilter));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.mipmapMode));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.addressModeU));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.addressModeV));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.addressModeW));
					Utility::HashCombine(hash, info.mipLodBias);
					Utility::HashCombine(hash, static_cast<uint32_t>(info.anisotropyEnable));
					Utility::HashCombine(hash, info.maxAnisotropy);
					Utility::HashCombine(hash, static_cast<uint32_t>(info.compareEnable));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.compareOp));
					Utility::HashCombine(hash, info.minLod);
					Utility::HashCombine(hash, info.maxLod);
					Utility::HashCombine(hash, static_cast<uint32_t>(info.borderColor));
					Utility::HashCombine(hash, static_cast<uint32_t>(info.unnormalizedCoordinates));

					if (info.pNext) {
						vk::StructureType type = *static_cast<const vk::StructureType*>(info.pNext);
						switch (type) {
						case vk::StructureType::eSamplerBlockMatchWindowCreateInfoQCOM:
							{
								const auto* data = static_cast<const vk::SamplerBlockMatchWindowCreateInfoQCOM*>(info.pNext);
								Utility::HashCombine(hash, data->windowExtent.height);
								Utility::HashCombine(hash, data->windowExtent.width);
								Utility::HashCombine(hash, static_cast<uint32_t>(data->windowCompareMode));
							}
						case vk::StructureType::eSamplerBorderColorComponentMappingCreateInfoEXT:
							{
								const auto* data = static_cast<const vk::SamplerBorderColorComponentMappingCreateInfoEXT*>(info.pNext);
								Utility::HashCombine(hash, data->components.r);
								Utility::HashCombine(hash, data->components.g);
								Utility::HashCombine(hash, data->components.b);
								Utility::HashCombine(hash, data->components.a);
								Utility::HashCombine(hash, static_cast<uint32_t>(data->srgb));
							}
						case vk::StructureType::eSamplerCubicWeightsCreateInfoQCOM:
							{
								const auto* data = static_cast<const vk::SamplerCubicWeightsCreateInfoQCOM*>(info.pNext);
								Utility::HashCombine(hash, static_cast<uint32_t>(data->cubicWeights));
							}
						case vk::StructureType::eSamplerCustomBorderColorCreateInfoEXT:
							{
								const auto* data = static_cast<const vk::SamplerCustomBorderColorCreateInfoEXT*>(info.pNext);
								Utility::HashCombine(hash, data->customBorderColor.uint32[0]);
								Utility::HashCombine(hash, data->customBorderColor.uint32[1]);
								Utility::HashCombine(hash, data->customBorderColor.uint32[2]);
								Utility::HashCombine(hash, data->customBorderColor.uint32[3]);
								Utility::HashCombine(hash, static_cast<uint32_t>(data->format));
							}
						case vk::StructureType::eSamplerReductionModeCreateInfo:
							{
								const auto* data = static_cast<const vk::SamplerReductionModeCreateInfo*>(info.pNext);
								Utility::HashCombine(hash, static_cast<uint32_t>(data->reductionMode));
							}
						default: {}
						}
					}

					return hash;
				}
			};

			std::vector<vk::Sampler> m_Samplers;
			std::unordered_map<vk::SamplerCreateInfo, SamplerHandle, SCIHasher> m_SamplerMap;


			static constexpr vk::ImageLayout s_DstLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };

			static std::unique_ptr<VulkanTextureManager> s_Instance;

		};
	}
}
