#include "VulkanMaterialSystem.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanMaterialSystem> VulkanMaterialSystem::s_Instance{ nullptr };

		VulkanMaterialSystem::VulkanMaterialSystem() {
			m_Materials.Reserve(512);

			m_PlaceholderMaterial = m_HandleAllocator.Allocate();
			m_HandleAllocator.AddRef(m_PlaceholderMaterial);

			MaterialData placeholderData{
				.albedoTexture = Core::AssetRef(VulkanTextureManager::GetPlaceholder<Texture2>()),
				.albedoSampler = 0
			};

			m_Materials.EmplaceAt(m_PlaceholderMaterial.GetIndex(), Material{ .customData = std::move(placeholderData), .shaderEffect = Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>()), .version = m_PlaceholderMaterial.GetVersion() });
		}

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

		void VulkanMaterialSystem::BindAsset(const Core::Handle<Material> handle, const Core::AssetID id, const uint32_t vectorKey) {
			return Get().m_HandleAllocator.BindAsset(handle, id, vectorKey);
		}

		uint32_t VulkanMaterialSystem::BumpGeneration(const Core::Handle<Material> handle) {
			return Get().m_HandleAllocator.BumpGeneration(handle);
		}

		bool VulkanMaterialSystem::IsHandleValid(const Core::ConstHandle<Material> handle) {
			return Get().m_HandleAllocator.IsHandleValid(handle);
		}

		Core::AssetID VulkanMaterialSystem::GetAssetID(const Core::Handle<Material> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Material handle passed to GetAssetID is invalid.");
			return Get().m_HandleAllocator.GetBoundAssetID(handle);
		}

		bool VulkanMaterialSystem::TryAddRef(const Core::Handle<Material> handle) {
			return Get().m_HandleAllocator.TryAddRef(handle);
		}

		void VulkanMaterialSystem::AddRef(const Core::Handle<Material> handle) {
			Get().m_HandleAllocator.AddRef(handle);
		}

		void VulkanMaterialSystem::RemoveRef(const Core::Handle<Material> handle) {
			Get().m_HandleAllocator.RemoveRef(handle);
		}

		void VulkanMaterialSystem::SetAssetStatus(const Core::Handle<Material> handle, const AssetStatus newStatus) {
			Get().m_HandleAllocator.SetAssetStatus(handle, newStatus);
		}

		AssetStatus VulkanMaterialSystem::GetAssetStatus(const Core::Handle<Material> handle) {
			return Get().m_HandleAllocator.GetAssetStatus(handle);
		}

		void VulkanMaterialSystem::RegisterAtSlot(const Core::Handle<Material> handle) {
			//empty cuz materials are guaranteed to be loaded next frame. Like the compute shaders they are kind of an exception, but for a different reason, to avoid constant rebatching in the scene renderer.
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

		void VulkanMaterialSystem::Unload(const Core::Handle<Material> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UNLOAD (releasing the shader effect and albedo texture refs, freeing handle)", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion());
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
			CORI_CORE_ASSERT(handle != Get().m_PlaceholderMaterial, "Placeholder material handle was passed, can't unload it.")

			Core::AssetID id = Get().m_HandleAllocator.GetBoundAssetID(handle);
			{
				auto& mutex = Core::AssetManager2::GetMutex();
				std::lock_guard lk(mutex);
				CORI_PROFILER_LOCK_MARK(mutex);
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				uint32_t vectorKey = record.vectorKey;
				auto old = Core::Handle<Material>(Core::AssetManager2::GetRawHandle(vectorKey));
				if (old == handle) {
					Core::AssetManager2::SetRawHandle(vectorKey, handle.ToRaw());
				}
			}

			Get().m_HandleAllocator.Free(handle);

			if (!Get().m_Materials.IsIndexOccupied(handle.GetIndex())) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, no material slot was occupied");
				return;
			}

			Get().m_Materials.RemoveAt(handle.GetIndex());
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, slot released (shader effect and albedo texture refs dropped, stale data left in the material slot map buffer)");
		}

		void VulkanMaterialSystem::QueueUnload(const Core::Handle<Material> handle) {
			RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
		}

		void VulkanMaterialSystem::AddOnShaderEffectSwappedListener(void* instance, OnShaderEffectSwappedFn func) {
			Get().m_OnShaderEffectSwappedListeners.emplace_back(instance, std::move(func));
		}

		void VulkanMaterialSystem::RemoveOnShaderEffectSwappedListener(const void* instance) {
			std::vector<std::pair<void*, OnShaderEffectSwappedFn>>::iterator result;
			bool isFound = false;
			for (auto it = Get().m_OnShaderEffectSwappedListeners.begin(); it != Get().m_OnShaderEffectSwappedListeners.end(); it++) {
				if (it->first == instance) {
					result = it;
					isFound = true;
					break;
				}
			}

			if (isFound) {
				Get().m_OnShaderEffectSwappedListeners.erase(result);
			}
		}

		void VulkanMaterialSystem::ClearOnShaderEffectSwappedListeners() {
			Get().m_OnShaderEffectSwappedListeners.clear();
		}

		void VulkanMaterialSystem::Sync() {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Process);
			Get().m_Materials.Sync();
		}

		uint64_t VulkanMaterialSystem::GetMaterialSlotMapBDA() {
			return Get().m_Materials.GetVulkanBuffer().GetBDA();
		}

		void VulkanMaterialSystem::CreateMaterial(const Core::Handle<Material> handle, Core::AssetRef<ShaderEffect> shaderEffect, MaterialData data) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			[[maybe_unused]] const auto shaderEffectHandle = shaderEffect.GetHandle();
			[[maybe_unused]] const auto albedoTextureHandle = data.albedoTexture.GetHandle();
			[[maybe_unused]] const auto albedoSampler = data.albedoSampler;

			auto& material = m_Materials[handle];
			material.shaderEffect = std::move(shaderEffect);
			material.customData = std::move(data);

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u], shader effect handle=[%u, %u], albedo texture handle=[%u, %u], albedo sampler=%u", handle.GetIndex(), handle.GetVersion(), shaderEffectHandle.GetIndex(), shaderEffectHandle.GetVersion(), albedoTextureHandle.GetIndex(), albedoTextureHandle.GetVersion(), albedoSampler);
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Material created directly into the slot (no streaming, no asset file)");
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] CREATED in place (shader effect handle=[%u, %u], albedo texture handle=[%u, %u])", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), shaderEffectHandle.GetIndex(), shaderEffectHandle.GetVersion(), albedoTextureHandle.GetIndex(), albedoTextureHandle.GetVersion());
		}

		Core::Handle<Material> VulkanMaterialSystem::DuplicateMaterial(const Core::Handle<Material> handle, const char* name) {
			//if (!IsHandleValid(material)) {
			//	CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to DuplicateMaterial, returning placeholder material.");
			//	return Get().m_PlaceholderMaterial;
			//}
//
			//const auto& data = std::as_const(Get().m_Materials)[material];
			//auto handle = Get().m_Materials.Emplace(Material{ .customData = data.customData, .shaderEffect = data.shaderEffect });
			//return handle;

			CORI_CORE_ASSERT(false, "Not implemented");
			return {};
		}

		std::expected<std::reference_wrapper<const MaterialData>, ErrorCode> VulkanMaterialSystem::GetMaterialData(const Core::ConstHandle<Material> handle) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			const auto& material = std::as_const(Get().m_Materials)[handle];
			return std::cref(material.customData);
		}

		std::expected<void, ErrorCode> VulkanMaterialSystem::ChangeMaterialData(const Core::Handle<Material> handle, MaterialData&& data) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			Get().m_Materials[handle].customData = std::move(data);
			return {};
		}

		std::expected<std::reference_wrapper<const Core::AssetRef<ShaderEffect>>, ErrorCode> VulkanMaterialSystem::GetMaterialShaderEffect(const Core::ConstHandle<Material> handle) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			const auto& material = std::as_const(Get().m_Materials)[handle];

			return std::cref(material.shaderEffect);
		}

		std::expected<void, ErrorCode> VulkanMaterialSystem::ChangeMaterialShaderEffect(const Core::Handle<Material> handle, Core::AssetRef<ShaderEffect> newShaderEffect) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			if (!newShaderEffect.IsInitialized()) {
				return std::unexpected(ErrorCode::eUninitializedAssetRef);
			}

			auto& material = Get().m_Materials[handle];

			if (!Get().m_OnShaderEffectSwappedListeners.empty()) {
				for (auto& [ptr, func] : Get().m_OnShaderEffectSwappedListeners) {
					func(ptr, handle, material.shaderEffect.GetHandle(), newShaderEffect.GetHandle());
				}
			}

			material.shaderEffect = std::move(newShaderEffect);

			return {};
		}

		void VulkanMaterialSystem::AssignPlaceholder(const Core::Handle<Material> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			auto& data = m_Materials[handle];
			auto& placeholderData = std::as_const(m_Materials)[m_PlaceholderMaterial];
			data.shaderEffect = placeholderData.shaderEffect;
			data.customData = placeholderData.customData;

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> PLACEHOLDER material (copied from handle [%u, %u], placeholder shader effect and albedo texture refd)", handle.GetIndex(), handle.GetVersion(), m_PlaceholderMaterial.GetIndex(), m_PlaceholderMaterial.GetVersion());
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned PLACEHOLDER material (copied from handle [%u, %u])", CORI_CLEAN_TYPE_NAME(Material), handle.GetIndex(), handle.GetVersion(), m_PlaceholderMaterial.GetIndex(), m_PlaceholderMaterial.GetVersion());
		}
	}
}