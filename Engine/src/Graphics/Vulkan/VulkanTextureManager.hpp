#pragma once
#include "VulkanEngine.hpp"
#include "VulkanImage.hpp"
#include "Graphics/Image.hpp"
#include "VulkanLayoutManager.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "VulkanFlagsGlaze.hpp"

namespace Cori {
	namespace Graphics {
		using SamplerHandle = uint32_t;
		class VulkanTextureManager;

		struct Texture2 : Core::SecondaryAssetBase {
			using Manager = VulkanTextureManager;
			VulkanImage image;
			vk::ImageView view;
			uint32_t descriptorIndex{ 0 };
			Core::AssetID assetID{ 0 };
			Core::AssetDeletionPolicy deletionPolicy{};
			uint32_t dataVersion{ 0 };
			bool placeholderAssigned{ false };
			bool loaded{ false };
		};

		class VulkanTextureManager {
			struct QueuedUpload {
				VulkanStreamingLine::ImageUpload imageUpload;
				std::vector<Byte> data;
				Core::Handle<Texture2> texture;
				uint32_t dataVersion{ 0 };
			};

			struct TextureInTransfer {
				VulkanImage image;
				Core::Handle<Texture2> texture;
				vk::ImageSubresourceLayers subresource;
				uint32_t dataVersion{ 0 };
			};

			struct InTransferSlot {
				uint64_t ticket{ 0 };
				std::vector<TextureInTransfer> texturesInTransfer;
			};

			struct JsonAssetData {
				std::string image;
			};

			struct JsonAssetDataCombined {
				glz::skip Metadata;
				JsonAssetData AssetData;
			};

		public:
			static void Init();

			static void Shutdown();

			static VulkanTextureManager& Get();

			//FIXME: returning a placeholder handle will lead to issues when trying to reload the texture under that handle, need to allocate a new handle on failure and passing a placeholder to it, and flag it as such to not delete it accidentally
			template<typename T> requires std::same_as<Texture2, T>
			static Core::Handle<T> Load(const Core::AssetID id) {
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				const auto& dir = Core::AssetManager2::GetAssetDir();

				auto handle = Get().AllocateHandle();
				Get().m_TexturePool[handle].assetID = id;
				Get().m_TexturePool[handle].deletionPolicy = record.deletionPolicy;
				record.rawHandleIndex = handle.GetIndex();
				record.rawHandleVersion = handle.GetVersion();

				auto assetFilePath = dir / record.path;

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Load({}), handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::enum_to_string(readError));
					Get().AssignPlaceholder(handle);
					return handle;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Load({}), handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::format_error(parseError, buffer));
					Get().AssignPlaceholder(handle);
					return handle;
				}

				//this way of loading a texture is temporary, 1st it always converts to rgba8, 2nd if the load is queued i have to copy the pixel data, it would be much better to have the ability to take ownership of it. Will need to more away from SDL3_image

				auto image = Image::Create(assetFilePath.replace_filename(data.AssetData.image));

				Get().CreateTexture(handle, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, { image->GetHeight(), image->GetWidth(), 1 }, 1, 1, vk::SampleCountFlagBits::e1, "test");
				Get().UpdateTexture(handle, std::span{ static_cast<Byte*>(image->GetPixelData()), image->GetHeight() * image->GetWidth() * 4 }, { 0, 0, 0 }, { image->GetHeight(), image->GetWidth(), 1 }, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 });
				Get().ChangeView(handle, vk::ImageViewType::e2D, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				return handle;
			}

			static void Reload(const Core::Handle<Texture2> handle, const Core::AssetID id) {
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				record.status = AssetStatus::eUnloaded;
				const auto& dir = Core::AssetManager2::GetAssetDir();
				bool deleted = ChangeDeletionPolicy(handle, record.deletionPolicy);
				if (deleted) {
					return;
				}

				auto assetFilePath = dir / record.path;
				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Reload([{},{}], {}) failed to open asset file '{}', error '{}', asset will not be reloaded.", handle.GetIndex(), handle.GetVersion(), id, assetFilePath.string(), glz::enum_to_string(readError));
					return;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Reload([{},{}], {}) failed to parse asset file '{}', asset will not be reloaded. Error: {}", handle.GetIndex(), handle.GetVersion(), id, assetFilePath.string(), glz::format_error(parseError, buffer));
					return;
				}

				auto& texture = Get().m_TexturePool[handle];
				if (texture.assetID != id) {
					record.rawHandleIndex = handle.GetIndex();
					record.rawHandleVersion = handle.GetVersion();
					auto& oldRecord = Core::AssetManager2::GetAssetRecord(texture.assetID);
					texture.assetID = id;
					oldRecord.rawHandleIndex = UINT32_MAX;
					oldRecord.rawHandleVersion = 0;
					oldRecord.status = AssetStatus::eUnloaded;
				}

				auto image = Image::Create(assetFilePath.replace_filename(data.AssetData.image));

				Get().DestroyTexture(handle);
				Get().CreateTexture(handle, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, { image->GetHeight(), image->GetWidth(), 1 }, 1, 1, vk::SampleCountFlagBits::e1, "test");
				Get().UpdateTexture(handle, std::span{ static_cast<Byte*>(image->GetPixelData()), image->GetHeight() * image->GetWidth() * 4 }, { 0, 0, 0 }, { image->GetHeight(), image->GetWidth(), 1 }, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 });
				Get().ChangeView(handle, vk::ImageViewType::e2D, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			}

			static void Unload(const Core::Handle<Texture2> handle) {
				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to Unload is invalid, skipping call.");
					return;
				}

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
				record.status = AssetStatus::eUnloaded;
				record.rawHandleIndex = UINT32_MAX;
				record.rawHandleVersion = 0;

				Get().DestroyTexture(handle);
				Get().FreeHandle(handle);
			}

			static bool ChangeDeletionPolicy(const Core::Handle<Texture2> handle, const Core::AssetDeletionPolicy newPolicy) {
				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to ChangeDeletionPolicy is invalid, skipping call.");
					return false;
				}

				auto& texture = Get().m_TexturePool[handle];
				if (texture.deletionPolicy == newPolicy) {
					return false;
				}

				if (texture.deletionPolicy == Core::AssetDeletionPolicy::eKeepAlive) {
					auto refCount = Get().m_TextureRefCounts[handle.GetIndex()];
					if (refCount == 0) {
						Unload(handle);
						return true;
					}
				}

				texture.deletionPolicy = newPolicy;
				return false;
			}

			static Core::AssetID GetAssetID(const Core::Handle<Texture2> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to GetAssetID in VulkanTextureManager is invalid.");

				return Get().m_TexturePool[handle].assetID;
			}

			template<typename T> requires std::same_as<Texture2, T>
			static Core::Handle<T> GetPlaceholder() {
				return Get().m_PlaceholderTexture;
			}

			static void AddRef(const Core::Handle<Texture2> handle) {
				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to AddRef is invalid, skipping call.");
					return;
				}

				Get().m_TextureRefCounts[handle.GetIndex()]++;
			}

			static void RemoveRef(const Core::Handle<Texture2> handle) {
				if (!Get().m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to RemoveRef is invalid, skipping call.");
					return;
				}

				auto count = --Get().m_TextureRefCounts[handle.GetIndex()];

				auto& texture = Get().m_TexturePool[handle.GetIndex()];
				if (count == 0 && texture.deletionPolicy == Core::AssetDeletionPolicy::eRefCounted && handle != Get().m_PlaceholderTexture) {
					Unload(handle);
				}
			}

			[[nodiscard]] static bool DoesSamplerExist(const char* alias) {
				return Get().m_SamplerAliases.contains(alias);
			}

			[[nodiscard]] static SamplerHandle GetSampler(const char* alias) {
				if (!Get().m_SamplerAliases.contains(alias)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "No sampler with alias '{}' found, returning placeholder.", alias);
					return 0;
				}

				return Get().m_SamplerAliases[alias];
			}

			[[nodiscard]] static SamplerHandle GetOrCreateSampler(const vk::SamplerCreateInfo& info, const char* alias = "") {
				auto [it, result] = Get().m_SamplerMap.try_emplace(info);
				bool aliasPresent = !CORI_IS_EMPTY_CSTR(alias);
				if (!result) {
					if (aliasPresent) {

						auto [it_, result_] = Get().m_SamplerAliases.try_emplace(alias);
						if (result_) {
							it_->second = it->second;
						} else if (it_->second != it->second) {
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "A different sampler with alias '{}' already exists, ignoring alias.", alias);
						}
					}

					return it->second;
				}

				uint32_t slot = Get().m_Samplers.size();

				if (slot >= Get().m_Samplers.size()) {
					Get().m_Samplers.resize(Get().m_Samplers.size() * 2.0f);
				}

				auto [result_, sampler] = VulkanEngine::GetLogicalDevice().createSampler(info);
				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create sampler. Error: {}", vk::to_string(result_));

				VulkanGlobalLayoutManager::UpdateSamplerDescriptor(slot, sampler);
				Get().m_Samplers[slot] = sampler;

				it->second = slot;

				if (aliasPresent) {
					auto [it_, result__] = Get().m_SamplerAliases.try_emplace(alias);
					if (result__) {
						it_->second = it->second;
					} else if (it_->second != it->second) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "A different sampler with alias '{}' already exists, ignoring alias.", alias);
					}
				}

				return slot;
			}

			[[nodiscard]] static bool AssignAliasToSampler(const SamplerHandle handle, const char* alias) {
				bool samplerFound = false;
				for (const auto samplerHandle : Get().m_SamplerMap | std::views::values) {
					if (samplerHandle == handle) {
						samplerFound = true;
						break;
					}
				}

				if (!samplerFound) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "No sampler with handle '{}' found, can't assign alias '{}' to a sampler that doesn't exist.", handle, alias);
					return false;
				}

				auto [it, result] = Get().m_SamplerAliases.try_emplace(alias);

				if (!result) {
					if (it->second == handle) {
						CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Sampler '{}' already have alias '{}' assigned to it.", handle, alias);
						return true;
					}

					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Alias '{}' is already assigned to different sampler than '{}', failed to assign alias.", alias, handle);
					return false;
				}

				it->second = handle;
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Assigned alias '{}' to sampler '{}'.", alias, handle);
				return true;
			}

			static void ProcessUpdates(vk::CommandBuffer cmb) {
				Get().m_BarrierCache.clear();
				auto currentTimelineValue = VulkanStreamingLine::GetTimelineValue();

				for (auto& [ticket, inTransferAssets] : Get().m_TexturesInTransfer) {
					if (currentTimelineValue >= ticket) {
						for (auto& inTransferTexture : inTransferAssets) {
							if (!Get().m_TexturePool.IsHandleValid(inTransferTexture.texture)) {
								DeletionQueue::PushImage(inTransferTexture.image);
								continue;
							}

							auto& texture = Get().m_TexturePool[inTransferTexture.texture];

							if (texture.dataVersion != inTransferTexture.dataVersion) {
								DeletionQueue::PushImage(inTransferTexture.image);
								continue;
							}

							Get().UpdateView(inTransferTexture.texture);

							vk::ImageSubresourceRange vkRange{
								.aspectMask = inTransferTexture.subresource.aspectMask,
								.baseMipLevel = inTransferTexture.subresource.mipLevel,
								.levelCount = 1,
								.baseArrayLayer = inTransferTexture.subresource.baseArrayLayer,
								.layerCount = inTransferTexture.subresource.layerCount
							};

							Get().m_BarrierCache.emplace_back(vk::ImageMemoryBarrier2{
								.srcStageMask = vk::PipelineStageFlagBits2::eNone,
								.srcAccessMask = vk::AccessFlagBits2::eNone,
								.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
								.dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
								.oldLayout = s_DstLayout,
								.newLayout = s_DstLayout,
								.srcQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex(),
								.dstQueueFamilyIndex = VulkanEngine::GetGraphicsQueueFamilyIndex(),
								.image = texture.image.m_Image,
								.subresourceRange = vkRange
							});

							texture.loaded = true;

							Core::AssetManager2::GetAssetRecord(GetAssetID(inTransferTexture.texture)).status = AssetStatus::eLoaded;
						}

						inTransferAssets.clear();
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
					if (!Get().m_TexturePool.IsHandleValid(upload.texture)) {
						Get().m_QueuedUploads.pop();
						continue;
					}

					auto& texture = Get().m_TexturePool[upload.texture];
					if (upload.dataVersion != texture.dataVersion) {
						Get().m_QueuedUploads.pop();
						continue;
					}

					VulkanStreamingLine::GenericUpload request{
						.resourceUpload = upload.imageUpload,
						.data = upload.data
					};

					auto result = VulkanStreamingLine::SubmitUploads(request);

					if (result) {
						Get().FindInTransferSlot(result.value()).emplace_back(TextureInTransfer{
							.image = upload.imageUpload.resource,
							.texture = upload.texture,
							.subresource = upload.imageUpload.range.subresourceLayers,
							.dataVersion = upload.dataVersion
						});

						Core::AssetManager2::GetAssetRecord(GetAssetID(upload.texture)).status = AssetStatus::eLoading;
						Get().m_QueuedUploads.pop();
					} else {
						break;
					}
				}

				Get().m_GPUAssetTables.Sync();
			}

			[[nodiscard]] static uint64_t GetTextureAssetTableBDA() {
				return Get().m_GPUAssetTables.GetVulkanBuffer().GetBDA();
			}

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<Texture2> handle) {
				return Get().m_TexturePool.IsHandleValid(handle);
			}

			~VulkanTextureManager() {
				for (auto& texture : m_TexturePool) {
					if (texture.image.m_Image) {
						DeletionQueue::PushImage(texture.image);
					}
				}

				for (auto& sampler : m_Samplers) {
					if (sampler) {
						VulkanEngine::GetLogicalDevice().destroySampler(sampler);
					}
				}
			}

			static constexpr bool EnableHotReload = true;

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
				m_TextureRefCounts.resize(VulkanGlobalLayoutManager::GetMaxTextures());
				m_Samplers.reserve(VulkanGlobalLayoutManager::GetMaxSamplers());

				m_GPUAssetTables.Resize(VulkanGlobalLayoutManager::GetMaxTextures());
				m_FreeTextureDescriptorSlots.resize(VulkanGlobalLayoutManager::GetMaxTextures() - 2);

				uint32_t counter = VulkanGlobalLayoutManager::GetMaxTextures() - 1;
				for (auto& index : m_FreeTextureDescriptorSlots) {
					index = counter--;
				}

				Texture2 missingTexturePlaceholder;
				Texture2 whiteTexture;

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

					missingData[i * 4] = isBlack ? 0x00 : 0xFF;
					missingData[i * 4 + 1] = 0x00;
					missingData[i * 4 + 2] = isBlack ? 0x00 : 0xFF;
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

				auto& table = m_GPUAssetTables[m_PlaceholderTexture.GetIndex()];
				table.descriptorIndex = s_PlaceholderTextureDescriptorIndex;
				table.version = m_PlaceholderTexture.GetVersion();

				auto& table_ = m_GPUAssetTables[m_WhiteTexture.GetIndex()];
				table_.descriptorIndex = s_WhiteTextureDescriptorIndex;
				table_.version = m_WhiteTexture.GetVersion();

				CORI_CORE_ASSERT(ticket, "Failed to submit uploads for white or placeholder texture to streaming line. Error: ", to_string(ticket.error()));

				VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket.value(), vk::PipelineStageFlagBits::eAllCommands);

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
				m_SamplerAliases.insert({ "Default", 0});

				LoadSamplers();
			}

			void AssignPlaceholder(const Core::Handle<Texture2> handle) {
				if (!m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to AssignPlaceholderTexture is invalid, skipping call.");
					return;
				}

				auto& texture = m_TexturePool[handle];
				texture.placeholderAssigned = true;

				auto& tableEntry = m_GPUAssetTables[handle.GetIndex()];
				tableEntry.descriptorIndex = s_PlaceholderTextureDescriptorIndex;
				tableEntry.version = handle.GetVersion();
			}

			[[nodiscard]] Core::Handle<Texture2> GetWhiteTexture() {
				return m_WhiteTexture;
			}

			void UpdateTexture(const Core::Handle<Texture2> handle, std::vector<Byte>&& pixels, const vk::Offset3D& offset, const vk::Extent3D& extent, const vk::ImageSubresourceLayers& subresourceLayers) {
				if (!(extent.width > 0 && extent.height > 0 && extent.depth > 0)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Invalid texture extent passed to UpdateTexture, skipping call.");
					return;
				}

				if (!m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to UpdateTexture is invalid, skipping call.");
					return;
				}

				auto& texture = m_TexturePool[handle];

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));

				if (record.status != AssetStatus::eUnloaded) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to UpdateTexture is pointing to an already loaded texture, skipping call.");
					return;
				}

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
					FindInTransferSlot(result.value()).emplace_back(TextureInTransfer{
						.image = texture.image,
						.texture = handle,
						.subresource = subresourceLayers,
						.dataVersion = texture.dataVersion
					});

					record.status = AssetStatus::eLoading;
				} else {
					if (result.error() == ErrorCode::eInvalidData) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "VulkanStreamingLine returned with error code eInvalidData during UpdateTexture call, skipping call.");
						record.status = AssetStatus::eLoadFailed;
						return;
					}

					m_QueuedUploads.emplace(resUpload, std::move(pixels), handle);
					record.status = AssetStatus::eLoadQueued;
				}
			}

			void UpdateTexture(const Core::Handle<Texture2> handle, const std::span<Byte>& pixels, const vk::Offset3D& offset, const vk::Extent3D& extent, const vk::ImageSubresourceLayers& subresourceLayers) {
				if (!(extent.width > 0 && extent.height > 0 && extent.depth > 0)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Invalid texture extent passed to UpdateTexture, skipping call.");
					return;
				}

				if (!m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to UpdateTexture is invalid, skipping call.");
					return;
				}

				auto& texture = m_TexturePool[handle];

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));

				if (record.status != AssetStatus::eUnloaded) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to UpdateTexture is pointing to an already loaded texture, skipping call.");
					return;
				}

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
					FindInTransferSlot(result.value()).emplace_back(TextureInTransfer{
						.image = texture.image,
						.texture = handle,
						.subresource = subresourceLayers,
						.dataVersion = texture.dataVersion
					});

					record.status = AssetStatus::eLoading;
				} else {
					if (result.error() == ErrorCode::eInvalidData) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "VulkanStreamingLine returned with error code eInvalidData during UpdateTexture call, skipping call.");
						record.status = AssetStatus::eLoadFailed;
						return;
					}

					std::vector<Byte> dataCopy;
					dataCopy.resize(pixels.size());
					memcpy(dataCopy.data(), pixels.data(), pixels.size());

					m_QueuedUploads.emplace(resUpload, std::move(dataCopy), handle);
					record.status = AssetStatus::eLoadQueued;
				}
			}

			void ChangeView(const Core::Handle<Texture2> handle, const vk::ImageViewType viewType, const vk::ImageSubresourceRange& subresourceRange) {
				if (!m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to ChangeView is invalid, skipping call.");
					return;
				}

				auto& texture = m_TexturePool[handle];

				VulkanImage::ImageViewKey viewKey{
					.type = viewType,
					.subresourceRange = subresourceRange
				};

				texture.view = texture.image.GetView(viewKey);
			}

			void UpdateView(const Core::Handle<Texture2> handle) {
				if (!m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to UpdateView is invalid, skipping call.");
					return;
				}

				auto& texture = m_TexturePool[handle];

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, texture.view);
			}

			Core::Handle<Texture2> AllocateHandle() {
				return m_TexturePool.Emplace();
			}

			void CreateTexture(const Core::Handle<Texture2> handle, const vk::ImageType type, const vk::Format format, const vk::Extent3D& extent, const uint32_t mipCount, const uint32_t layerCount, const vk::SampleCountFlagBits sampleFlags, const char* name = "") {
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

				auto& texture = m_TexturePool[handle];

				texture.image = VulkanImage::Create(info);
				texture.descriptorIndex = m_FreeTextureDescriptorSlots.back();
				m_FreeTextureDescriptorSlots.pop_back();

				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, m_TexturePool[m_WhiteTexture].view);

				if (handle.GetIndex() >= m_GPUAssetTables.Size()) {
					m_GPUAssetTables.Resize(m_GPUAssetTables.Size() * 1.5f);
				}

				if (handle.GetIndex() >= m_TextureRefCounts.size()) {
					m_TextureRefCounts.resize(m_TextureRefCounts.size() * 1.5f);
				}

				auto& table = m_GPUAssetTables[handle.GetIndex()];
				table.descriptorIndex = texture.descriptorIndex;
				table.version = handle.GetVersion();
			}

			void DestroyTexture(const Core::Handle<Texture2> handle) {
				if (!m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to DestroyTexture is invalid, skipping call.");
					return;
				}

				if (handle == m_PlaceholderTexture || handle == m_WhiteTexture) {
					return;
				}

				auto& texture = m_TexturePool[handle];

				if (!texture.placeholderAssigned && texture.loaded) {
					DeletionQueue::PushImage(texture.image);
					VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, m_TexturePool[m_WhiteTexture].view);
				}

				m_FreeTextureDescriptorSlots.emplace_back(texture.descriptorIndex);

				auto& table = m_GPUAssetTables[handle.GetIndex()];
				table.descriptorIndex = 0;

				texture.image = {};
				texture.view = VK_NULL_HANDLE;
				texture.descriptorIndex = 0;
				texture.placeholderAssigned = false;
				texture.loaded = false;
				texture.dataVersion++;
			}

			void FreeHandle(const Core::Handle<Texture2> handle) {
				if (!m_TexturePool.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Handle passed to FreeHandle is invalid, skipping call.");
					return;
				}

				if (handle == m_PlaceholderTexture || handle == m_WhiteTexture) {
					return;
				}

				m_TextureRefCounts[handle.GetIndex()] = 0;
				m_TexturePool.Remove(handle);
			}

			std::vector<TextureInTransfer>& FindInTransferSlot(const uint64_t value) {
				InTransferSlot* free = nullptr;
				InTransferSlot* bestPick = nullptr;

				for (auto& pair : m_TexturesInTransfer) {
					if (pair.ticket == value) {
						bestPick = &pair;
						break;
					}

					if (pair.texturesInTransfer.empty()) {
						free = &pair;
					}
				}

				if (bestPick) {
					bestPick->ticket = value;
					return bestPick->texturesInTransfer;
				}

				free->ticket = value;
				return free->texturesInTransfer;
			}

			struct SamplerJsonDef {
				std::string Alias;

				vk::SamplerCreateFlags Flags;
				vk::Filter MagFilter;
				vk::Filter MinFilter;
				vk::SamplerAddressMode AddressModeU;
				vk::SamplerAddressMode AddressModeV;
				vk::SamplerAddressMode AddressModeW;
				float MipLoadBias;
				bool AnisotropyEnable;
				float MaxAnisotropy;
				bool CompareEnable;
				vk::CompareOp CompareOp;
				float MinLod;
				float MaxLod;
				vk::BorderColor BorderColor;
				bool UnnormalizedCoordinates;

				std::optional<uint32_t> internalValue;
			};

			void LoadSamplers() {
				std::filesystem::path config = FileSystem::PathManager::GetAliasedPath("ASSET_DIR") / "Samplers.json";

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, config.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Failed to open sampler config file '{}', skipping it.", config.string());
				}

				//the fallback here is only for glaze to not bitch about a single sampler object in the json that failed to parse, so it doesnt abort parsing.
				//Even tho it will say that it got a fallback sampler in the console when parsing fails, the sampler wont't be actually added, i rely on GetSampler to return a default sampler when it cant find a sampler with the given alias.
				std::vector<Utility::GlazeWithFallback<SamplerJsonDef, []{ return SamplerJsonDef{ .internalValue = UINT32_MAX }; }, "Sampler config in VulkanTextureManager. Fallback is just to skip this one, the actual vulkan sampler will not be created from this fallback entry.">> samplers;

				auto parseError = glz::read<Utility::ReflectEnumsOpts{ .error_on_missing_keys = true }>(samplers, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Failed to parse sampler configs from 'Samplers.json', error: {}", glz::format_error(parseError, buffer));
					return;
				}

				for (auto& def : samplers) {
					if (def->internalValue != UINT32_MAX) {
						vk::SamplerCreateInfo info{
							.flags = def->Flags,
							.magFilter = def->MagFilter,
							.minFilter = def->MinFilter,
							.addressModeU = def->AddressModeU,
							.addressModeV = def->AddressModeV,
							.addressModeW = def->AddressModeW,
							.mipLodBias = def->MipLoadBias,
							.anisotropyEnable = def->AnisotropyEnable,
							.maxAnisotropy = def->MaxAnisotropy,
							.compareEnable = def->CompareEnable,
							.compareOp = def->CompareOp,
							.minLod = def->MinLod,
							.maxLod = def->MaxLod,
							.borderColor = def->BorderColor,
							.unnormalizedCoordinates = def->UnnormalizedCoordinates
						};

						static_cast<void>(GetOrCreateSampler(info, def->Alias.c_str()));
					}
				}
			}

			std::vector<uint32_t> m_TextureRefCounts;

			Core::FlatSlotMap<Texture2> m_TexturePool;

			std::vector<uint32_t> m_FreeTextureDescriptorSlots;

			struct GPUAssetTable {
				uint32_t descriptorIndex{ 0 };
				uint32_t version{ 0 };
			};

			Core::Handle<Texture2> m_PlaceholderTexture;
			Core::Handle<Texture2> m_WhiteTexture;

			static constexpr uint32_t s_PlaceholderTextureDescriptorIndex{ 0 };
			static constexpr uint32_t s_WhiteTextureDescriptorIndex{ 1 };

			VulkanDynamicVector<GPUAssetTable> m_GPUAssetTables{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Texture Asset Look Up Table" };

			std::vector<vk::ImageMemoryBarrier2> m_BarrierCache;

			std::array<InTransferSlot, TRANSFERS_IN_FLIGHT + 1> m_TexturesInTransfer;
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

			struct TransparentHash {
				using is_transparent = void;

				size_t operator()(std::string_view sv) const noexcept {
					return std::hash<std::string_view>{}(sv);
				}
			};

			struct TransparentEqual {
				using is_transparent = void;

				bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
					return lhs == rhs;
				}
			};

			std::unordered_map<std::string, SamplerHandle, TransparentHash, TransparentEqual> m_SamplerAliases;

			static constexpr vk::ImageLayout s_DstLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };

			static std::unique_ptr<VulkanTextureManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Texture2, Graphics);
	}
}
