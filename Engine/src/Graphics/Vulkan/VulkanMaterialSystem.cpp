#include "VulkanMaterialSystem.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanMaterialSystem> VulkanMaterialSystem::s_Instance{ nullptr };

		void VulkanMaterialSystem::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanMaterialSystem is already initialized.");
			s_Instance = std::unique_ptr<VulkanMaterialSystem>(new VulkanMaterialSystem());
		}

		void VulkanMaterialSystem::Shutdown() {
			s_Instance.reset();
		}

		VulkanMaterialSystem& VulkanMaterialSystem::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanMaterialSystem::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void VulkanMaterialSystem::Load(const Core::Handle<Material> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			auto future = Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, vectorKey]() mutable -> WorkerPayload {
				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Load({}), material handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					return WorkerPayload{ std::nullopt };
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Load({}), material handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					return WorkerPayload{ std::nullopt };
				}

				return WorkerPayload{WorkerPayloadData{
					.colorFactor = {data.AssetData.materialData.colorFactor[0], data.AssetData.materialData.colorFactor[1], data.AssetData.materialData.colorFactor[2], data.AssetData.materialData.colorFactor[3]},
					.albedoTexture = std::move(data.AssetData.materialData.albedoTexture),
					.albedoSampler = std::move(data.AssetData.materialData.albedoSampler),
					.shaderEffect = std::move(data.AssetData.shaderEffect)
				}};
			});

			RenderThreadCommandQueue::Push([handle, gen, future = std::move(future), vectorKey]() mutable {
				if (!IsHandleValid(handle)) {
					return;
				}

				if (Get().m_HandleAllocator.GetGeneration(handle) != gen) {
					return;
				}

				Core::ConstHandle<ShaderEffect> oldEffect;
				Core::ConstHandle<ShaderEffect> newEffect;
				bool isOccupied = false;
				if (Get().m_Materials.IsIndexOccupied(handle.GetIndex())) {
					oldEffect = std::as_const(Get().m_Materials)[handle].shaderEffect.GetHandle();
					isOccupied = true;
				}

				auto payload = future.get();
				if (!payload.actualPayload) {
					if (isOccupied) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Reload for material handle [{},{}] failed, keeping whatever was loaded there before.", handle.GetIndex(), handle.GetVersion());
					} else {
						Get().m_Materials.EmplaceAt(handle.GetIndex(), Material{
							.customData = MaterialData{
								.albedoTexture = Core::AssetRef(VulkanTextureManager::GetPlaceholder<Texture2>()),
								.albedoSampler = 0 },
							.shaderEffect = Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>()),
							.version = handle.GetVersion()
						});
					}

					SetAssetStatus(handle, AssetStatus::eLoadFailed);
				} else {
					if (isOccupied) {
						newEffect = payload.actualPayload->shaderEffect.GetHandle();

						VulkanShaderEffectManager::RegisterAtSlot(payload.actualPayload->shaderEffect.GetHandle());
						VulkanTextureManager::RegisterAtSlot(payload.actualPayload->albedoTexture.GetHandle());
						auto& material = Get().m_Materials[handle];
						material.customData = MaterialData{
							.colorFactor = { payload.actualPayload->colorFactor[0], payload.actualPayload->colorFactor[1], payload.actualPayload->colorFactor[2], payload.actualPayload->colorFactor[3] },
							.albedoTexture = std::move(payload.actualPayload->albedoTexture),
							.albedoSampler = VulkanTextureManager::GetSampler(payload.actualPayload->albedoSampler.c_str())
						};

						material.shaderEffect = std::move(payload.actualPayload->shaderEffect);
						material.version = handle.GetVersion();

						if (oldEffect != newEffect) {
							if (!Get().m_OnShaderEffectSwappedListeners.empty()) {
								for (auto& [ptr, func] : Get().m_OnShaderEffectSwappedListeners) {
									func(ptr, handle, oldEffect, newEffect);
								}
							}
						}
					} else {
						VulkanShaderEffectManager::RegisterAtSlot(payload.actualPayload->shaderEffect.GetHandle());
						VulkanTextureManager::RegisterAtSlot(payload.actualPayload->albedoTexture.GetHandle());
						Get().m_Materials.EmplaceAt(handle.GetIndex(), Material{
							.customData = MaterialData{
								.colorFactor = { payload.actualPayload->colorFactor[0], payload.actualPayload->colorFactor[1], payload.actualPayload->colorFactor[2], payload.actualPayload->colorFactor[3] },
								.albedoTexture = std::move(payload.actualPayload->albedoTexture),
								.albedoSampler = VulkanTextureManager::GetSampler(payload.actualPayload->albedoSampler.c_str()) },
							.shaderEffect = std::move(payload.actualPayload->shaderEffect),
							.version = handle.GetVersion()
						});
					}

					SetAssetStatus(handle, AssetStatus::eLoaded);
				}
			});
		}
	}
}