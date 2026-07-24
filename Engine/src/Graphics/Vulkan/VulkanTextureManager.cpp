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
			RenderThreadCommandQueue::Push([handle]() mutable {
				if (!IsHandleValid(handle)) {
					return;
				}

				if (Get().m_TexturePool.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				Get().m_TexturePool.EmplaceAt(handle.GetIndex());
				Get().AssignPlaceholder(handle);
			});
		}

		void VulkanTextureManager::Load(const Core::Handle<Texture2> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			RegisterAtSlot(handle);
			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				auto FinalizeLoad = [](const Core::Handle<Texture2> handle_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayload, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([handle_, gen_, payload = std::move(payload.value())]() mutable {
							if (!IsHandleValid(handle_)) {
								return;
							}

							if (Get().m_HandleAllocator.GetGeneration(handle_) != gen_) {
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

							auto& table = Get().m_GPUAssetTables[handle_.GetIndex()];
							table.descriptorIndex = texture.descriptorIndex;
							table.version = handle_.GetVersion();

							bool success = Get().UpdateTexture(handle_, std::move(payload.m_PixelData), { 0, 0, 0 }, { texture.image.m_Extent3D.width, texture.image.m_Extent3D.height, texture.image.m_Extent3D.depth }, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, gen_);
							if (success) {
								Get().ChangeView(handle_, vk::ImageViewType::e2D, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
							} else {
								Get().AssignPlaceholder(handle_);
							}

							payload.Release();
						});
					}
					else {
						RenderThreadCommandQueue::Push([handle_, gen_, vectorKey_]() {
							if (!IsHandleValid(handle_)) {
								return;
							}

							if (Get().m_HandleAllocator.GetGeneration(handle_) != gen_) {
								return;
							}

							if (!Get().m_TexturePool.IsIndexOccupied(handle_.GetIndex())) {
								Get().m_TexturePool.EmplaceAt(handle_.GetIndex());
								Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
						});
					}
				};

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Load({}), handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::TextureManager }, "Load({}), handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
					return;
				}

				//this way of loading a texture is temporary and very much suboptimal.

				auto image = Image::Create(path.replace_filename(data.AssetData.image));
				CORI_CORE_ASSERT(image->GetWidth() > 0 && image->GetHeight() > 0, "Invalid image extent.");
				uint64_t pixelDatSize = image->GetWidth() * image->GetHeight() * 4;

				//later when i make a proper image loading, the image allocation can run in parallel with the image parsing, we get the image metadata, spin a worker with high priority (tbb) that creates the texture
				vk::ImageCreateInfo imageCreateInfo {
					.imageType = vk::ImageType::e2D,
					.format = vk::Format::eR8G8B8A8Srgb,
					.extent = { image->GetWidth(), image->GetHeight(), 1 },
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


				std::vector<Byte> pixelData;
				pixelData.resize(pixelDatSize);
				memcpy(pixelData.data(), image->GetPixelData(), pixelDatSize);
				WorkerPayload payload(VulkanImage::Create(info), std::move(pixelData));

				FinalizeLoad(handle, gen, vectorKey, std::move(payload));
			});


		}
	}
}