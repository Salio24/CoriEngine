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
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] LOAD requested (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

			auto future = Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable -> WorkerPayload {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Worker Task: Material parse", Cori::ProfileColors::Worker);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u path=%s", handle.GetIndex(), handle.GetVersion(), gen, path.string().c_str());
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] worker begin (parse), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Load({}), material handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), handing an empty payload to the render thread", to_string(ErrorCode::eFailedToOpenFile).data());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: cannot open asset file '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
					return WorkerPayload{ std::nullopt };
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Load({}), material handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), handing an empty payload to the render thread", to_string(ErrorCode::eParseFailure).data());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: asset json '%s' parse error, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
					return WorkerPayload{ std::nullopt };
				}

				[[maybe_unused]] const auto shaderEffectHandle = data.AssetData.shaderEffect.GetHandle();
				[[maybe_unused]] const auto albedoTextureHandle = data.AssetData.materialData.albedoTexture.GetHandle();
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Parsed: shader effect handle=[%u, %u], albedo texture handle=[%u, %u], albedo sampler alias='%s'", shaderEffectHandle.GetIndex(), shaderEffectHandle.GetVersion(), albedoTextureHandle.GetIndex(), albedoTextureHandle.GetVersion(), data.AssetData.materialData.albedoSampler.c_str());
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Parsed: colorFactor=[%.3f, %.3f, %.3f, %.3f]", static_cast<double>(data.AssetData.materialData.colorFactor[0]), static_cast<double>(data.AssetData.materialData.colorFactor[1]), static_cast<double>(data.AssetData.materialData.colorFactor[2]), static_cast<double>(data.AssetData.materialData.colorFactor[3]));

				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Parsed, handed to the render thread through the future");
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Decode, "%s Handle=[%u, %u] parsed material (shader effect handle=[%u, %u], albedo texture handle=[%u, %u]), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), shaderEffectHandle.GetIndex(), shaderEffectHandle.GetVersion(), albedoTextureHandle.GetIndex(), albedoTextureHandle.GetVersion(), gen, static_cast<unsigned long long>(id));

				return WorkerPayload{WorkerPayloadData{
					.colorFactor = {data.AssetData.materialData.colorFactor[0], data.AssetData.materialData.colorFactor[1], data.AssetData.materialData.colorFactor[2], data.AssetData.materialData.colorFactor[3]},
					.albedoTexture = std::move(data.AssetData.materialData.albedoTexture),
					.albedoSampler = std::move(data.AssetData.materialData.albedoSampler),
					.shaderEffect = std::move(data.AssetData.shaderEffect)
				}};
			});

			RenderThreadCommandQueue::Push([handle, id, gen, future = std::move(future), vectorKey]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Material FinalizeLoad", Cori::ProfileColors::Finalize);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

				if (!IsHandleValid(handle)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, dropped parsed material (shader effect and albedo texture refs released by the payload dtor)");
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: handle invalid (parsed material discarded), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
					return;
				}

				const uint32_t currentGen = Get().m_HandleAllocator.GetGeneration(handle);
				if (currentGen != gen) {
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), dropped parsed material (shader effect and albedo texture refs released by the payload dtor)", gen, currentGen);
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), gen, currentGen, static_cast<unsigned long long>(id));
					return;
				}

				Core::ConstHandle<ShaderEffect> oldEffect;
				Core::ConstHandle<ShaderEffect> newEffect;
				bool isOccupied = false;
				if (Get().m_Materials.IsIndexOccupied(handle.GetIndex())) {
					oldEffect = std::as_const(Get().m_Materials)[handle].shaderEffect.GetHandle();
					isOccupied = true;
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Slot was occupied: this is a RELOAD, old shader effect handle=[%u, %u]", oldEffect.GetIndex(), oldEffect.GetVersion());
				} else {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Slot was free: this is a first load");
				}

				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Wait on material worker future", Cori::ProfileColors::Worker);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] blocking the render thread until the worker is done", handle.GetIndex(), handle.GetVersion());
					future.wait();
				}

				auto payload = future.get();
				if (!payload.actualPayload) {
					if (isOccupied) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Reload for material handle [{},{}] failed, keeping whatever was loaded there before.", handle.GetIndex(), handle.GetVersion());
						CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: status=LoadFailed, RELOAD failed, kept the material that was already in the slot");
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] RELOAD FAILED, status=LoadFailed, kept the previously loaded material, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
					} else {
						Get().m_Materials.EmplaceAt(handle.GetIndex(), Material{
							.customData = MaterialData{
								.albedoTexture = Core::AssetRef(VulkanTextureManager::GetPlaceholder<Texture2>()),
								.albedoSampler = 0 },
							.shaderEffect = Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>()),
							.version = handle.GetVersion()
						});

						CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: status=LoadFailed, emplaced PLACEHOLDER shader effect + MISSING albedo texture");
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED, status=LoadFailed, PLACEHOLDER shader effect + MISSING albedo texture shown, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
					}

					SetAssetStatus(handle, AssetStatus::eLoadFailed);
				} else {
					[[maybe_unused]] const auto shaderEffectHandle = payload.actualPayload->shaderEffect.GetHandle();
					[[maybe_unused]] const auto albedoTextureHandle = payload.actualPayload->albedoTexture.GetHandle();
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Shader effect handle=[%u, %u], albedo texture handle=[%u, %u], albedo sampler alias='%s'", shaderEffectHandle.GetIndex(), shaderEffectHandle.GetVersion(), albedoTextureHandle.GetIndex(), albedoTextureHandle.GetVersion(), payload.actualPayload->albedoSampler.c_str());

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

						CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Slot was occupied: overwrote it with the parsed material");

						if (oldEffect != newEffect) {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Notify shader effect swapped listeners", Cori::ProfileColors::Bind);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Shader effect SWAPPED [%u, %u] -> [%u, %u], notifying %llu listener(s)", oldEffect.GetIndex(), oldEffect.GetVersion(), newEffect.GetIndex(), newEffect.GetVersion(), static_cast<unsigned long long>(Get().m_OnShaderEffectSwappedListeners.size()));
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Bind, "%s Handle=[%u, %u] shader effect SWAPPED [%u, %u] -> [%u, %u], notifying %llu listener(s) (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), oldEffect.GetIndex(), oldEffect.GetVersion(), newEffect.GetIndex(), newEffect.GetVersion(), static_cast<unsigned long long>(Get().m_OnShaderEffectSwappedListeners.size()), gen, static_cast<unsigned long long>(id));

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

						CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Slot was free: emplaced the parsed material");
					}

					SetAssetStatus(handle, AssetStatus::eLoaded);
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: LOADED, real material now usable");
					CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded, "%s Handle=[%u, %u] LOADED, real material now usable (shader effect handle=[%u, %u], albedo texture handle=[%u, %u]) (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), shaderEffectHandle.GetIndex(), shaderEffectHandle.GetVersion(), albedoTextureHandle.GetIndex(), albedoTextureHandle.GetVersion(), gen, static_cast<unsigned long long>(id));
				}
			});
		}
	}
}