#include "VulkanTextureManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanTextureManager> VulkanTextureManager::s_Instance{ nullptr };

		VulkanTextureManager::VulkanTextureManager() {
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
			m_PlaceholderView = missingTexturePlaceholder.view;
			m_WhiteView = whiteTexture.view;

			VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(s_PlaceholderTextureDescriptorIndex, m_PlaceholderView);
			VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(s_WhiteTextureDescriptorIndex, m_WhiteView);

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

			m_PlaceholderTexture = m_HandleAllocator.Allocate();
			m_WhiteTexture = m_HandleAllocator.Allocate();

			m_HandleAllocator.AddRef(m_PlaceholderTexture);
			m_HandleAllocator.AddRef(m_WhiteTexture);

			m_TexturePool.EmplaceAt(m_PlaceholderTexture.GetIndex(), missingTexturePlaceholder);
			m_TexturePool.EmplaceAt(m_WhiteTexture.GetIndex(), whiteTexture);

			auto& table = m_GPUAssetTables[m_PlaceholderTexture.GetIndex()];
			table.descriptorIndex = s_PlaceholderTextureDescriptorIndex;
			table.version = m_PlaceholderTexture.GetVersion();

			auto& table_ = m_GPUAssetTables[m_WhiteTexture.GetIndex()];
			table_.descriptorIndex = s_WhiteTextureDescriptorIndex;
			table_.version = m_WhiteTexture.GetVersion();

			CORI_CORE_ASSERT(ticket, "Failed to submit uploads for white or placeholder texture to streaming line. Error: ", to_string(ticket.error()));

			VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket.value(), vk::PipelineStageFlagBits::eAllCommands);

			for (uint32_t i = 2; i < VulkanGlobalLayoutManager::GetMaxTextures() - 2; i++) {
				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(i, m_PlaceholderView);
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
			m_SamplerAliases.insert({ "Default", 0 });

			LoadSamplers();
		}

		VulkanTextureManager::~VulkanTextureManager() {
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

		void VulkanTextureManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanTextureManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanTextureManager>(new VulkanTextureManager());
		}

		void VulkanTextureManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanTextureManager& VulkanTextureManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanTextureManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void VulkanTextureManager::AllocateExtras(const Core::Handle<Texture2> handle) {
			CORI_CORE_ASSERT(handle.GetIndex() < VulkanGlobalLayoutManager::GetMaxTextures(), "Out of texture slots");
		}

		void VulkanTextureManager::BindAsset(const Core::Handle<Texture2> handle, const Core::AssetID id, const uint32_t vectorKey) {
			Get().m_HandleAllocator.BindAsset(handle, id, vectorKey);
		}

		uint32_t VulkanTextureManager::BumpGeneration(const Core::Handle<Texture2> handle) {
			return Get().m_HandleAllocator.BumpGeneration(handle);
		}

		bool VulkanTextureManager::IsHandleValid(const Core::Handle<Texture2> handle) {
			return Get().m_HandleAllocator.IsHandleValid(handle);
		}

		Core::AssetID VulkanTextureManager::GetAssetID(const Core::Handle<Texture2> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to GetAssetID in VulkanTextureManager is invalid.");
			return Get().m_HandleAllocator.GetBoundAssetID(handle);
		}

		bool VulkanTextureManager::TryAddRef(const Core::Handle<Texture2> handle) {
			return Get().m_HandleAllocator.TryAddRef(handle);
		}

		void VulkanTextureManager::AddRef(const Core::Handle<Texture2> handle) {
			Get().m_HandleAllocator.AddRef(handle);
		}

		void VulkanTextureManager::RemoveRef(const Core::Handle<Texture2> handle) {
			Get().m_HandleAllocator.RemoveRef(handle);
		}

		void VulkanTextureManager::SetAssetStatus(const Core::Handle<Texture2> handle, const AssetStatus newStatus) {
			Get().m_HandleAllocator.SetAssetStatus(handle, newStatus);
		}

		AssetStatus VulkanTextureManager::GetAssetStatus(const Core::Handle<Texture2> handle) {
			return Get().m_HandleAllocator.GetAssetStatus(handle);
		}

		void VulkanTextureManager::RegisterAtSlot(const Core::Handle<Texture2> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Register);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

			RenderThreadCommandQueue::Push([handle]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Texture RegisterAtSlot", Cori::ProfileColors::Register);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

				if (!IsHandleValid(handle)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid");
					return;
				}


				if (Get().m_TexturePool.IsIndexOccupied(handle.GetIndex())) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Index already occupied");
					return;
				}

				Get().m_TexturePool.EmplaceAt(handle.GetIndex());
				Get().AssignWhitePlaceholder(handle);
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Emplaced and WHITE placeholder assigned");
			});
		}

		void VulkanTextureManager::Load(const Core::Handle<Texture2> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name /*""*/) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] LOAD requested (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

			RegisterAtSlot(handle);
			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Worker Task: Texture parse/decode/creation", Cori::ProfileColors::Worker);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u path=%s", handle.GetIndex(), handle.GetVersion(), gen, path.string().c_str());
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] worker begin (parse/decode/create), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

				auto FinalizeLoad = [](const Core::Handle<Texture2> handle_, const Core::AssetID id_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayload, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([id_, handle_, gen_, payload = std::move(payload.value())]() mutable {
							//auto payload = std::move(payload_);
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Texture FinalizeLoad", Cori::ProfileColors::Finalize);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));

							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, dropped decoded image");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: handle invalid (decoded image discarded), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_HandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), dropped decoded image", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
								return;
							}

							if (!Get().m_TexturePool.IsIndexOccupied(handle_.GetIndex())) {
								Get().m_TexturePool.EmplaceAt(handle_.GetIndex());
							}

							Get().DestroyTexture(handle_);
							auto& texture = Get().m_TexturePool[handle_];
							texture.placeholderAssigned = false;
							texture.image = payload.m_Image;
							texture.descriptorIndex = Get().m_FreeTextureDescriptorSlots.back();
							Get().m_FreeTextureDescriptorSlots.pop_back();

							VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, Get().m_WhiteView);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Took descriptor slot %u, bound WHITE view (upload pending)", texture.descriptorIndex);
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Finalize, "%s Handle=[%u, %u] finalize: took descriptor %u, bound WHITE (upload pending) (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle_.GetIndex(), handle_.GetVersion(), texture.descriptorIndex, gen_, static_cast<unsigned long long>(id_));

							auto& table = Get().m_GPUAssetTables[handle_.GetIndex()];
							table.descriptorIndex = texture.descriptorIndex;
							table.version = handle_.GetVersion();

							bool success = Get().UpdateTexture(handle_, std::move(payload.m_PixelData), { 0, 0, 0 }, { texture.image.m_Extent3D.width, texture.image.m_Extent3D.height, texture.image.m_Extent3D.depth }, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, gen_);
							if (success) {
								Get().ChangeView(handle_, vk::ImageViewType::e2D, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Upload submitted, real view binds on transfer complete");
							} else {
								Get().AssignPlaceholder(handle_);
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: UpdateTexture FAILED, reverted to MISSING placeholder");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] finalize: UpdateTexture FAILED, reverted to MISSING placeholder, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
							}

							payload.Release();
						});
					}
					else {
						RenderThreadCommandQueue::Push([id_, handle_, gen_, vectorKey_]() {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Texture FinalizeLoad (fail)", Cori::ProfileColors::Destroy);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, skipped");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: handle invalid, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_HandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), skipped", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
								return;
							}

							if (!Get().m_TexturePool.IsIndexOccupied(handle_.GetIndex())) {
								Get().m_TexturePool.EmplaceAt(handle_.GetIndex());
								Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: status=LoadFailed, MISSING placeholder shown");
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED, status=LoadFailed, MISSING placeholder shown, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
						});
					}
				};

				JsonAssetDataCombined data;
				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Texture json parse", Cori::ProfileColors::Decode);
					std::string buffer;
					auto readError = glz::file_to_buffer(buffer, path.c_str());
					if (readError != glz::error_code::none) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Load({}), handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eFailedToOpenFile).data());
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: cannot open asset file '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
						FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
						return;
					}

					auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
					if (parseError) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Load({}), handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eParseFailure).data());
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: asset json '%s' parse error, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
						FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
						return;
					}
				}

				//this way of loading a texture is temporary and very much suboptimal.

				std::vector<Byte> pixelData;
				uint32_t width;
				uint32_t height;
				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Image load", Cori::ProfileColors::Decode);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Image file: %s", data.AssetData.image.c_str());
					auto image = Image::Create(path.replace_filename(data.AssetData.image));
					CORI_CORE_ASSERT(image->GetWidth() > 0 && image->GetHeight() > 0, "Invalid image extent.");
					uint64_t pixelDatSize = image->GetWidth() * image->GetHeight() * 4;
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Decoded %ux%u (%llu bytes)", image->GetWidth(), image->GetHeight(), static_cast<unsigned long long>(pixelDatSize));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Decode, "%s Handle=[%u, %u] decoded image %ux%u (%llu bytes) (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), image->GetWidth(), image->GetHeight(), static_cast<unsigned long long>(pixelDatSize), gen, static_cast<unsigned long long>(id));
					pixelData.resize(pixelDatSize);
					memcpy(pixelData.data(), image->GetPixelData(), pixelDatSize);
					width = image->GetWidth();
					height = image->GetHeight();
				}

				//later when i make a proper image loading, the image allocation can run in parallel with the image parsing, we get the image metadata, spin a worker with high priority (tbb) that creates the texture
				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Texture allocation and FinalizeLoad submit", Cori::ProfileColors::Upload);
					vk::ImageCreateInfo imageCreateInfo {
						.imageType = vk::ImageType::e2D,
						.format = vk::Format::eR8G8B8A8Srgb,
						.extent = { width, height, 1 },
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

					VulkanImage::CreateInfo info {
						.imageCreateInfo = &imageCreateInfo,
						.allocationCreateInfo = &allocInfo,
					};

					if (!name.empty()) {
						info.name = name.c_str();
					}

					WorkerPayload payload(VulkanImage::Create(info), std::move(pixelData));

					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Decoded, handed to FinalizeLoad");
					FinalizeLoad(handle, id, gen, vectorKey, std::move(payload));
				}
			});
		}

		void VulkanTextureManager::Unload(const Core::Handle<Texture2> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UNLOAD (destroying texture, freeing handle)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion());
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
			CORI_CORE_ASSERT(handle != Get().m_PlaceholderTexture, "Placeholder texture handle was passed, can't unload it.")

			Core::AssetID id = Get().m_HandleAllocator.GetBoundAssetID(handle);
			{
				auto& mutex = Core::AssetManager2::GetMutex();
				std::lock_guard lk(mutex);
				CORI_PROFILER_LOCK_MARK(mutex);
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				if (record.rawHandleIndex == handle.GetIndex() && record.rawHandleVersion == handle.GetVersion()) {
					record.rawHandleIndex = UINT32_MAX;
					record.rawHandleVersion = 0;
				}
			}

			Get().m_HandleAllocator.Free(handle);

			if (!Get().m_TexturePool.IsIndexOccupied(handle.GetIndex())) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, no texture slot was occupied");
				return;
			}

			Get().DestroyTexture(handle);
			Get().m_TexturePool.RemoveAt(handle.GetIndex());
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, texture destroyed and slot released");
		}

		void VulkanTextureManager::QueueUnload(const Core::Handle<Texture2> handle) {
			RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
		}

		SamplerHandle VulkanTextureManager::GetOrCreateSampler(const vk::SamplerCreateInfo& info, const char* alias /*""*/) {
			std::lock_guard lk(Get().m_SamplerMutex);
			return Get().GetOrCreateSamplerImpl(info, alias);
		}

		SamplerHandle VulkanTextureManager::GetSampler(const char* alias) {
			std::lock_guard lk(Get().m_SamplerMutex);
			if (!Get().m_SamplerAliases.contains(alias)) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "No sampler with alias '{}' found, returning placeholder.", alias);
				return 0;
			}

			return Get().m_SamplerAliases[alias];
		}

		bool VulkanTextureManager::DoesSamplerExist(const char* alias) {
			std::lock_guard lk(Get().m_SamplerMutex);
			return Get().m_SamplerAliases.contains(alias);
		}

		bool VulkanTextureManager::AssignAliasToSampler(const SamplerHandle handle, const char* alias) {
			std::lock_guard lk(Get().m_SamplerMutex);
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

		void VulkanTextureManager::ProcessUpdates(vk::CommandBuffer cmb) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Process);
			Get().m_BarrierCache.clear();
			auto currentTimelineValue = VulkanStreamingLine::GetTimelineValue();

			for (auto& [ticket, inTransferAssets] : Get().m_TexturesInTransfer) {
				if (currentTimelineValue >= ticket) {
					for (auto& inTransferTexture : inTransferAssets) {
						CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "ProcessUpdates: Texture transfer complete", Cori::ProfileColors::Loaded);
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", inTransferTexture.texture.GetIndex(), inTransferTexture.texture.GetVersion());
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Ticket=%llu, Current timeline value=%llu", static_cast<unsigned long long>(ticket), static_cast<unsigned long long>(currentTimelineValue));
						if (!IsHandleValid(inTransferTexture.texture)) {
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, discarded transferred image");
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] transfer done but handle invalid, discarded image (ticket %llu)", CORI_CLEAN_TYPE_NAME(Texture2), inTransferTexture.texture.GetIndex(), inTransferTexture.texture.GetVersion(), static_cast<unsigned long long>(ticket));
							DeletionQueue::PushImage(inTransferTexture.image);
							continue;
						}

						auto& texture = Get().m_TexturePool[inTransferTexture.texture];

						if (Get().m_HandleAllocator.GetGeneration(inTransferTexture.texture) != inTransferTexture.loadGen) {
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (loadGen=%u, current gen=%u), discarded transferred image", inTransferTexture.loadGen, Get().m_HandleAllocator.GetGeneration(inTransferTexture.texture));
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] transfer STALE (loadGen=%u, current gen=%u) (ticket %llu), discarded image", CORI_CLEAN_TYPE_NAME(Texture2), inTransferTexture.texture.GetIndex(), inTransferTexture.texture.GetVersion(), inTransferTexture.loadGen, Get().m_HandleAllocator.GetGeneration(inTransferTexture.texture), static_cast<unsigned long long>(ticket));
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
							.oldLayout = vk::ImageLayout::eTransferDstOptimal,
							.newLayout = s_DstLayout,
							.srcQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex(),
							.dstQueueFamilyIndex = VulkanEngine::GetGraphicsQueueFamilyIndex(),
							.image = texture.image.m_Image,
							.subresourceRange = vkRange
						});

						texture.loaded = true;

						SetAssetStatus(inTransferTexture.texture, AssetStatus::eLoaded);
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: LOADED, real texture now visible (descriptor %u)", texture.descriptorIndex);
						CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded, "%s Handle=[%u, %u] LOADED, real texture now visible (descriptor %u) (ticket %llu)", CORI_CLEAN_TYPE_NAME(Texture2), inTransferTexture.texture.GetIndex(), inTransferTexture.texture.GetVersion(), texture.descriptorIndex, static_cast<unsigned long long>(ticket));
					}

					VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket, vk::PipelineStageFlagBits::eFragmentShader);

					inTransferAssets.clear();
				}
			}

			if (!Get().m_BarrierCache.empty()) {
				CORI_PROFILE_GPU_ZONE_CP(Cori::ProfileParts::RenderingAssets, VulkanEngine::GetGraphicsGPUProfilerContext(), cmb, "Texture Acquire Barriers", Cori::ProfileColors::GPUBarrier);

				vk::DependencyInfo depInfo{
					.imageMemoryBarrierCount = static_cast<uint32_t>(Get().m_BarrierCache.size()),
					.pImageMemoryBarriers = Get().m_BarrierCache.data()
				};

				cmb.pipelineBarrier2(depInfo);
			}

			while (!Get().m_QueuedUploads.empty()) {
				auto& upload = Get().m_QueuedUploads.front();
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "ProcessUpdates: Queued upload retry", Cori::ProfileColors::Upload);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", upload.texture.GetIndex(), upload.texture.GetVersion());

				if (!IsHandleValid(upload.texture)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, queued upload dropped");
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] queued upload dropped (handle invalid) (gen=%u)", CORI_CLEAN_TYPE_NAME(Texture2), upload.texture.GetIndex(), upload.texture.GetVersion(), upload.loadGen);
					upload.imageUpload.resource.Destroy();
					Get().m_QueuedUploads.pop();
					continue;
				}

				if (upload.loadGen != Get().m_HandleAllocator.GetGeneration(upload.texture)) {
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (loadGen=%u, current gen=%u), queued upload dropped", upload.loadGen, Get().m_HandleAllocator.GetGeneration(upload.texture));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] queued upload dropped (STALE loadGen=%u, current gen=%u)", CORI_CLEAN_TYPE_NAME(Texture2), upload.texture.GetIndex(), upload.texture.GetVersion(), upload.loadGen, Get().m_HandleAllocator.GetGeneration(upload.texture));
					upload.imageUpload.resource.Destroy();
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
						.loadGen = upload.loadGen
					});

					SetAssetStatus(upload.texture, AssetStatus::eStreaming);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: SUBMITTED (ticket=%llu), status=Streaming", static_cast<unsigned long long>(result.value()));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] queued upload SUBMITTED (ticket=%llu) (gen=%u), status=Streaming", CORI_CLEAN_TYPE_NAME(Texture2), upload.texture.GetIndex(), upload.texture.GetVersion(), static_cast<unsigned long long>(result.value()), upload.loadGen);

					Get().m_QueuedUploads.pop();
				} else {
					break;
				}
			}

			Get().m_GPUAssetTables.Sync();
		}

		uint64_t VulkanTextureManager::GetTextureAssetTableBDA() {
			return Get().m_GPUAssetTables.GetVulkanBuffer().GetBDA();
		}

		void VulkanTextureManager::CreateTexture(const Core::Handle<Texture2> handle, const vk::ImageType type, const vk::Format format, const vk::Extent3D& extent, const uint32_t mipCount, const uint32_t layerCount, const vk::SampleCountFlagBits sampleFlags, const char* name /*""*/) {
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

			VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, m_WhiteView);

			auto& table = m_GPUAssetTables[handle.GetIndex()];
			table.descriptorIndex = texture.descriptorIndex;
			table.version = handle.GetVersion();
		}

		bool VulkanTextureManager::UpdateTexture(const Core::Handle<Texture2> handle, std::vector<Byte>&& pixels, const vk::Offset3D& offset, const vk::Extent3D& extent, const vk::ImageSubresourceLayers& subresourceLayers, const uint32_t loadGen) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Upload);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] loadGen=%u", handle.GetIndex(), handle.GetVersion(), loadGen);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			auto& texture = m_TexturePool[handle];

			if (!(extent.width > 0 && extent.height > 0 && extent.depth > 0)) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Invalid texture extent passed to UpdateTexture, skipping load.");
				SetAssetStatus(handle, AssetStatus::eLoadFailed);
				DestroyTexture(handle);
				return false;
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
					.loadGen = loadGen
				});

				SetAssetStatus(handle, AssetStatus::eStreaming);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Upload SUBMITTED to streaming line (ticket=%llu), status=Streaming", static_cast<unsigned long long>(result.value()));
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] upload SUBMITTED to streaming line (ticket=%llu), status=Streaming", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), static_cast<unsigned long long>(result.value()));
			} else {
				if (result.error() == ErrorCode::eInvalidData) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "VulkanStreamingLine returned with error code eInvalidData during UpdateTexture call, skipping load.");
					SetAssetStatus(handle, AssetStatus::eLoadFailed);
					DestroyTexture(handle);
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line returned eInvalidData, status=LoadFailed");
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UpdateTexture FAILED: streaming line eInvalidData, status=LoadFailed", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion());
					return false;
				}

				m_QueuedUploads.emplace(resUpload, std::move(pixels), handle, loadGen);
				SetAssetStatus(handle, AssetStatus::eStreamingQueued);
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line full, upload QUEUED, status=StreamingQueued (still sampling WHITE)");
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] upload QUEUED (streaming line full), status=StreamingQueued", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion());
			}

			return true;
		}

		void VulkanTextureManager::DestroyTexture(const Core::Handle<Texture2> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			if (handle == m_PlaceholderTexture || handle == m_WhiteTexture) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Skipped: placeholder textures are never destroyed");
				return;
			}

			auto& texture = m_TexturePool[handle];

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] descriptor %u (placeholderAssigned=%d, loaded=%d)", handle.GetIndex(), handle.GetVersion(), texture.descriptorIndex, static_cast<int>(texture.placeholderAssigned), static_cast<int>(texture.loaded));
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] DestroyTexture (descriptor %u, placeholderAssigned=%d, loaded=%d)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), texture.descriptorIndex, static_cast<int>(texture.placeholderAssigned), static_cast<int>(texture.loaded));

			if (!texture.placeholderAssigned && texture.loaded) {
				DeletionQueue::PushImage(texture.image);
				//if (texture.image.m_Image) {
				//}
			}

			if (texture.descriptorIndex != s_PlaceholderTextureDescriptorIndex && texture.descriptorIndex != s_WhiteTextureDescriptorIndex) {
				VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, m_PlaceholderView);
				m_FreeTextureDescriptorSlots.emplace_back(texture.descriptorIndex);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Released descriptor %u (rebound to MISSING placeholder view, slot returned to free list)", texture.descriptorIndex);
			}

			//AssignPlaceholder(handle);

			texture.image = {};
			texture.view = nullptr;
			texture.loaded = false;
		}

		void VulkanTextureManager::ChangeView(const Core::Handle<Texture2> handle, const vk::ImageViewType viewType, const vk::ImageSubresourceRange& subresourceRange) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			auto& texture = m_TexturePool[handle];

			VulkanImage::ImageViewKey viewKey{
				.type = viewType,
				.subresourceRange = subresourceRange
			};

			texture.view = texture.image.GetView(viewKey);
		}

		void VulkanTextureManager::UpdateView(const Core::Handle<Texture2> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			auto& texture = m_TexturePool[handle];

			VulkanGlobalLayoutManager::UpdateSampledTextureDescriptor(texture.descriptorIndex, texture.view);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] descriptor %u -> REAL texture view (transfer done)", handle.GetIndex(), handle.GetVersion(), texture.descriptorIndex);
		}

		void VulkanTextureManager::AssignPlaceholder(const Core::Handle<Texture2> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			auto& texture = m_TexturePool[handle];
			texture.placeholderAssigned = true;
			texture.descriptorIndex = s_PlaceholderTextureDescriptorIndex;

			auto& tableEntry = m_GPUAssetTables[handle.GetIndex()];
			tableEntry.descriptorIndex = s_PlaceholderTextureDescriptorIndex;
			tableEntry.version = handle.GetVersion();

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> MISSING placeholder (descriptor %u)", handle.GetIndex(), handle.GetVersion(), s_PlaceholderTextureDescriptorIndex);
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned MISSING placeholder (descriptor %u)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), s_PlaceholderTextureDescriptorIndex);
		}

		void VulkanTextureManager::AssignWhitePlaceholder(const Core::Handle<Texture2> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::White);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			auto& texture = m_TexturePool[handle];
			texture.placeholderAssigned = true;
			texture.descriptorIndex = s_WhiteTextureDescriptorIndex;

			auto& tableEntry = m_GPUAssetTables[handle.GetIndex()];
			tableEntry.descriptorIndex = s_WhiteTextureDescriptorIndex;
			tableEntry.version = handle.GetVersion();

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> WHITE placeholder (descriptor %u)", handle.GetIndex(), handle.GetVersion(), s_WhiteTextureDescriptorIndex);
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::White, "%s Handle=[%u, %u] assigned WHITE placeholder (descriptor %u)", CORI_CLEAN_TYPE_NAME(Texture2), handle.GetIndex(), handle.GetVersion(), s_WhiteTextureDescriptorIndex);
		}

		std::vector<VulkanTextureManager::TextureInTransfer>& VulkanTextureManager::FindInTransferSlot(const uint64_t value) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Upload);
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

			CORI_CORE_ASSERT(free, "Failed to find any free in transfer slot for a texture.");

			free->ticket = value;
			return free->texturesInTransfer;
		}

		SamplerHandle VulkanTextureManager::GetOrCreateSamplerImpl(const vk::SamplerCreateInfo& info, const char* alias /*""*/) {
			auto [it, result] = m_SamplerMap.try_emplace(info);
			bool aliasPresent = !CORI_IS_EMPTY_CSTR(alias);
			if (!result) {
				if (aliasPresent) {

					auto [it_, result_] = m_SamplerAliases.try_emplace(alias);
					if (result_) {
						it_->second = it->second;
					} else if (it_->second != it->second) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "A different sampler with alias '{}' already exists, ignoring alias.", alias);
					}
				}

				return it->second;
			}

			auto [result_, sampler] = VulkanEngine::GetLogicalDevice().createSampler(info);
			CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create sampler. Error: {}", vk::to_string(result_));

			uint32_t slot = m_Samplers.size();
			m_Samplers.emplace_back(sampler);

			RenderThreadCommandQueue::Push([sampler, slot] {
				VulkanGlobalLayoutManager::UpdateSamplerDescriptor(slot, sampler);
			});

			it->second = slot;

			if (aliasPresent) {
				auto [it_, result__] = m_SamplerAliases.try_emplace(alias);
				if (result__) {
					it_->second = it->second;
				} else if (it_->second != it->second) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "A different sampler with alias '{}' already exists, ignoring alias.", alias);
				}
			}

			return slot;
		}

		void VulkanTextureManager::LoadSamplers() {
			std::lock_guard lk(m_SamplerMutex);
			std::filesystem::path config = FileSystem::PathManager::GetAliasedPath("ASSET_DIR") / "Samplers.json";

			std::string buffer;
			auto readError = glz::file_to_buffer(buffer, config.c_str());
			if (readError != glz::error_code::none) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Failed to open sampler config file '{}', skipping it.", config.string());
			}

			//the fallback here is only for glaze to not bitch about a single sampler object in the json that failed to parse, so it doesnt abort parsing.
			//Even tho it will say that it got a fallback sampler in the console when parsing fails, the sampler won't be actually added, i rely on GetSampler to return a default sampler when it cant find a sampler with the given alias.
			std::vector<Utility::GlazeWithFallback<SamplerJsonDef, []{ return SamplerJsonDef{ .internalValue = UINT32_MAX }; }, "Sampler config in VulkanTextureManager. Fallback is just to skip this one, the actual vulkan sampler will not be created from this fallback entry.">> samplers;

			auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(samplers, buffer);
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

					static_cast<void>(GetOrCreateSamplerImpl(info, def->Alias.c_str()));
				}
			}
		}
	}
}
