#include "VulkanTextureManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanTextureManager> VulkanTextureManager::s_Instance{ nullptr };

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

		void VulkanTextureManager::Load(const Core::Handle<Texture2> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
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
	}
}