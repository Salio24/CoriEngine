#include "VulkanShaderEffectManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanShaderEffectManager> VulkanShaderEffectManager::s_Instance{ nullptr };

		VulkanShaderEffectManager::VulkanShaderEffectManager() {
			m_ShaderEffects.Reserve(32);
			m_ShaderEffectData.Resize(32);

			m_PlaceholderEffect = m_HandleAllocator.Allocate();
			m_HandleAllocator.AddRef(m_PlaceholderEffect);

			m_ShaderEffects.EmplaceAt(m_PlaceholderEffect.GetIndex(), ShaderEffect{ .shaders = Core::AssetRef(VulkanShaderManager::GetPlaceholder<VertFragShaderPair>()) });
		}

		void VulkanShaderEffectManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderEffectManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanShaderEffectManager>(new VulkanShaderEffectManager());
		}

		void VulkanShaderEffectManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanShaderEffectManager& VulkanShaderEffectManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanShaderEffectManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void VulkanShaderEffectManager::BindAsset(const Core::Handle<ShaderEffect> handle, const Core::AssetID id, const uint32_t vectorKey) {
			return Get().m_HandleAllocator.BindAsset(handle, id, vectorKey);
		}

		uint32_t VulkanShaderEffectManager::BumpGeneration(const Core::Handle<ShaderEffect> handle) {
			return Get().m_HandleAllocator.BumpGeneration(handle);
		}

		bool VulkanShaderEffectManager::IsHandleValid(const Core::ConstHandle<ShaderEffect> handle) {
			return Get().m_HandleAllocator.IsHandleValid(handle);
		}

		Core::AssetID VulkanShaderEffectManager::GetAssetID(const Core::Handle<ShaderEffect> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "ShaderEffect handle passed to GetAssetID is invalid.");
			return Get().m_HandleAllocator.GetBoundAssetID(handle);
		}

		bool VulkanShaderEffectManager::TryAddRef(const Core::Handle<ShaderEffect> handle) {
			return Get().m_HandleAllocator.TryAddRef(handle);
		}

		void VulkanShaderEffectManager::AddRef(const Core::Handle<ShaderEffect> handle) {
			Get().m_HandleAllocator.AddRef(handle);
		}

		void VulkanShaderEffectManager::RemoveRef(const Core::Handle<ShaderEffect> handle) {
			Get().m_HandleAllocator.RemoveRef(handle);
		}

		void VulkanShaderEffectManager::SetAssetStatus(const Core::Handle<ShaderEffect> handle, const AssetStatus newStatus) {
			Get().m_HandleAllocator.SetAssetStatus(handle, newStatus);
		}

		AssetStatus VulkanShaderEffectManager::GetAssetStatus(const Core::Handle<ShaderEffect> handle) {
			return Get().m_HandleAllocator.GetAssetStatus(handle);
		}

		void VulkanShaderEffectManager::RegisterAtSlot(const Core::Handle<ShaderEffect> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Register);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

			RenderThreadCommandQueue::Push([handle]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Shader effect RegisterAtSlot", Cori::ProfileColors::Register);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

				if (!IsHandleValid(handle)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid");
					return;
				}

				if (Get().m_ShaderEffects.IsIndexOccupied(handle.GetIndex())) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Index already occupied");
					return;
				}

				uint32_t index = handle.GetIndex();
				uint32_t size = Get().m_ShaderEffectData.Size();
				if (index >= size) {
					Get().m_ShaderEffectData.Resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Grew shader effect data %u -> %llu entries (index %u)", size, static_cast<unsigned long long>(Get().m_ShaderEffectData.Size()), index);
				}

				Get().m_ShaderEffects.EmplaceAt(handle.GetIndex(), Get().m_ShaderEffects[Get().m_PlaceholderEffect]);
				//Get().AssignPlaceholder(handle);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Emplaced a copy of the PLACEHOLDER shader effect (from handle [%u, %u])", Get().m_PlaceholderEffect.GetIndex(), Get().m_PlaceholderEffect.GetVersion());
			});
		}

		void VulkanShaderEffectManager::Load(const Core::Handle<ShaderEffect> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] LOAD requested (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

			RegisterAtSlot(handle);
			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Worker Task: Shader effect parse", Cori::ProfileColors::Worker);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u path=%s", handle.GetIndex(), handle.GetVersion(), gen, path.string().c_str());
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] worker begin (parse), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

				auto FinalizeLoad = [](const Core::Handle<ShaderEffect> handle_, const Core::AssetID id_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayload, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([handle_, id_, gen_, payload = std::move(payload.value()), vectorKey_]() mutable {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Shader effect FinalizeLoad", Cori::ProfileColors::Finalize);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));

							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, dropped parsed shader effect (shader pair ref released by payload dtor)");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: handle invalid (parsed shader effect discarded), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_HandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), dropped parsed shader effect (shader pair ref released by payload dtor)", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
								return;
							}

							[[maybe_unused]] const auto shaderPairHandle = payload.effect.shaders.GetHandle();
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Shader pair handle=[%u, %u]", shaderPairHandle.GetIndex(), shaderPairHandle.GetVersion());

							if (!Get().m_ShaderEffects.IsIndexOccupied(handle_.GetIndex())) {
								uint32_t index = handle_.GetIndex();
								uint32_t size = Get().m_ShaderEffectData.Size();
								if (index >= size) {
									Get().m_ShaderEffectData.Resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
									CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Grew shader effect data %u -> %llu entries (index %u)", size, static_cast<unsigned long long>(Get().m_ShaderEffectData.Size()), index);
								}

								VulkanShaderManager::RegisterAtSlot(payload.effect.shaders.GetHandle());
								Get().m_ShaderEffects.EmplaceAt(handle_.GetIndex(), std::move(payload.effect));
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Slot was free: emplaced the parsed shader effect");
							} else {
								VulkanShaderManager::RegisterAtSlot(payload.effect.shaders.GetHandle());
								Get().m_ShaderEffects[handle_] = std::move(payload.effect);
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Slot was occupied (placeholder or previous effect): overwrote it with the parsed shader effect");
							}

							Get().m_ShaderEffectData[handle_.GetIndex()] = payload.data;

							SetAssetStatus(handle_, AssetStatus::eLoaded);
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: LOADED, real shader effect now usable");
							CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded, "%s Handle=[%u, %u] LOADED, real shader effect now usable (shader pair handle=[%u, %u]) (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle_.GetIndex(), handle_.GetVersion(), shaderPairHandle.GetIndex(), shaderPairHandle.GetVersion(), gen_, static_cast<unsigned long long>(id_));
						});
					}
					else {
						RenderThreadCommandQueue::Push([handle_, id_, gen_, vectorKey_]() {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Shader effect FinalizeLoad (fail)", Cori::ProfileColors::Destroy);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));

							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, skipped");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: handle invalid, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_HandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), skipped", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
								return;
							}

							if (!Get().m_ShaderEffects.IsIndexOccupied(handle_.GetIndex())) {
								uint32_t index = handle_.GetIndex();
								uint32_t size = Get().m_ShaderEffectData.Size();
								if (index >= size) {
									Get().m_ShaderEffectData.Resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
									CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Grew shader effect data %u -> %llu entries (index %u)", size, static_cast<unsigned long long>(Get().m_ShaderEffectData.Size()), index);
								}

								Get().m_ShaderEffects.EmplaceAt(handle_.GetIndex(), Get().m_ShaderEffects[Get().m_PlaceholderEffect]);
								//Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: status=LoadFailed, PLACEHOLDER shader effect shown");
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED, status=LoadFailed, PLACEHOLDER shader effect shown, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
						});
					}
				};

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Load({}), handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eFailedToOpenFile).data());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: cannot open asset file '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
					FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Load({}), handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eParseFailure).data());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: asset json '%s' parse error, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
					FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
					return;
				}

				ShaderEffectData customData{
					.custom1 = { data.AssetData.customData.custom1.value_or(0.0f), data.AssetData.customData.custom2.value_or(0.0f), data.AssetData.customData.custom3.value_or(0.0f), data.AssetData.customData.custom4.value_or(0.0f) },
					.custom2 = { data.AssetData.customData.custom5.value_or(0.0f), data.AssetData.customData.custom6.value_or(0.0f), data.AssetData.customData.custom7.value_or(0.0f), data.AssetData.customData.custom8.value_or(0.0f) }
				};

				PipelineState state{
					.cullMode = data.AssetData.pipelineState.cullMode,
					.frontFace = data.AssetData.pipelineState.frontFace,
					.depthCompareOp = data.AssetData.pipelineState.depthCompareOp,
					.depthTestEnable = data.AssetData.pipelineState.depthTestEnable,
					.depthWriteEnable = data.AssetData.pipelineState.depthWriteEnable,
					.depthBoundsTestEnable = data.AssetData.pipelineState.depthBoundsTestEnable,
					.depthBiasEnable = data.AssetData.pipelineState.depthBiasEnable,
					.stencilTestEnable = data.AssetData.pipelineState.stencilTestEnable,
					.logicOpEnable = data.AssetData.pipelineState.logicOpEnable
				};

				[[maybe_unused]] const auto shaderPairHandle = data.AssetData.shaderPair.GetHandle();
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Parsed: shader pair handle=[%u, %u], cullMode=%s, frontFace=%s, depthCompareOp=%s", shaderPairHandle.GetIndex(), shaderPairHandle.GetVersion(), vk::to_string(state.cullMode).c_str(), vk::to_string(state.frontFace).c_str(), vk::to_string(state.depthCompareOp).c_str());
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Parsed: depthTest=%d, depthWrite=%d, depthBoundsTest=%d, depthBias=%d, stencilTest=%d, logicOp=%d", static_cast<int>(state.depthTestEnable), static_cast<int>(state.depthWriteEnable), static_cast<int>(state.depthBoundsTestEnable), static_cast<int>(state.depthBiasEnable), static_cast<int>(state.stencilTestEnable), static_cast<int>(state.logicOpEnable));

				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Parsed, handed to FinalizeLoad");
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Decode, "%s Handle=[%u, %u] parsed shader effect (shader pair handle=[%u, %u]), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion(), shaderPairHandle.GetIndex(), shaderPairHandle.GetVersion(), gen, static_cast<unsigned long long>(id));

				FinalizeLoad(handle, id, gen, vectorKey, WorkerPayload{
					.effect = ShaderEffect{ .shaders = std::move(data.AssetData.shaderPair), .pipelineState = state },
					.data = customData
				});
			});
		}

		void VulkanShaderEffectManager::Unload(const Core::Handle<ShaderEffect> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UNLOAD (releasing the shader pair ref, freeing handle)", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion());
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
			CORI_CORE_ASSERT(handle != Get().m_PlaceholderEffect, "Placeholder shader effect handle was passed, can't unload it.")

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

			if (!Get().m_ShaderEffects.IsIndexOccupied(handle.GetIndex())) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, no shader effect slot was occupied");
				return;
			}

			Get().m_ShaderEffects.RemoveAt(handle.GetIndex());
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, slot released (shader pair ref dropped, stale data left in the shader effect data buffer)");
		}

		void VulkanShaderEffectManager::QueueUnload(const Core::Handle<ShaderEffect> handle) {
			RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
		}

		void VulkanShaderEffectManager::AddOnShaderEffectDeleteListener(void* instance, OnShaderEffectDeletedFn func) {
			Get().m_OnShaderEffectDeletedListeners.emplace_back(instance, std::move(func));
		}

		void VulkanShaderEffectManager::RemoveOnShaderEffectDeleteListener(const void* instance) {
			std::vector<std::pair<void*, OnShaderEffectDeletedFn>>::iterator result;
			bool isFound = false;
			for (auto it = Get().m_OnShaderEffectDeletedListeners.begin(); it != Get().m_OnShaderEffectDeletedListeners.end(); it++) {
				if (it->first == instance) {
					result = it;
					isFound = true;
					break;
				}
			}

			if (isFound) {
				Get().m_OnShaderEffectDeletedListeners.erase(result);
			}
		}

		void VulkanShaderEffectManager::ClearOnShaderEffectDeleteListeners() {
			Get().m_OnShaderEffectDeletedListeners.clear();
		}

		void VulkanShaderEffectManager::Sync() {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Process);
			Get().m_ShaderEffectData.Sync();
		}

		uint64_t VulkanShaderEffectManager::GetShaderEffectDataBufferBDA() {
			return Get().m_ShaderEffectData.GetVulkanBuffer().GetBDA();
		}

		void VulkanShaderEffectManager::CreateShaderEffect(const Core::Handle<ShaderEffect> handle, Core::AssetRef<VertFragShaderPair> shaderPair, const PipelineState& state, const ShaderEffectData& data) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid ShaderEffect handle passed to CreateShaderEffect");

			[[maybe_unused]] const auto shaderPairHandle = shaderPair.GetHandle();

			auto& effect = m_ShaderEffects[handle];
			effect.shaders = std::move(shaderPair);
			effect.pipelineState = state;

			m_ShaderEffectData[handle.GetIndex()] = data;

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u], shader pair handle=[%u, %u], cullMode=%s, frontFace=%s, depthCompareOp=%s", handle.GetIndex(), handle.GetVersion(), shaderPairHandle.GetIndex(), shaderPairHandle.GetVersion(), vk::to_string(state.cullMode).c_str(), vk::to_string(state.frontFace).c_str(), vk::to_string(state.depthCompareOp).c_str());
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Shader effect created directly into the slot (no streaming, no asset file)");
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] CREATED in place (shader pair handle=[%u, %u])", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion(), shaderPairHandle.GetIndex(), shaderPairHandle.GetVersion());
		}

		std::expected<Core::Handle<ShaderEffect>, ErrorCode> VulkanShaderEffectManager::DuplicateShaderEffect(const Core::Handle<ShaderEffect> original, std::filesystem::path path, const Core::AssetDeletionPolicy deletionPolicy, std::string name) {
			//to properly implement this (and material duplication) I first have to add a new data path to the asset manager design, so far it has:
			// 1. Drive -> A.S. Registry -> Engine memory (initial load)
			// 2. Drive -> A.S. Registry -> Engine Memory (hot reload)
			// but i need a third part for serialization of some asset types e.g.
			// Request -> Engine memory + A.S. Registry -> Save to drive on request (saving the scene or smthg)
			// this will involve stuff like deciding on the new name (PBR_ShaderEffect Cope (1)) and
			// very likely some other complications that i don't see right now, but will see once i start designing the editor and the PrimaryAsset related systems.
			// So for that reason the 3rd asset data path will be put aside for now.

			CORI_CORE_ASSERT(false, "Not implemented");
			return {};
		}

		std::expected<std::reference_wrapper<const ShaderEffectData>, ErrorCode> VulkanShaderEffectManager::GetShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect) {
			if (!IsHandleValid(shaderEffect)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			return std::cref(std::as_const(Get().m_ShaderEffectData)[shaderEffect.GetIndex()]);
		}

		std::expected<void, ErrorCode> VulkanShaderEffectManager::ChangeShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect, const ShaderEffectData& data) {
			if (!IsHandleValid(shaderEffect)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			Get().m_ShaderEffectData[shaderEffect.GetIndex()] = data;
			return {};
		}

		std::expected<std::reference_wrapper<const Core::AssetRef<VertFragShaderPair>>, ErrorCode> VulkanShaderEffectManager::GetShaderEffectShaderPair(const Core::ConstHandle<ShaderEffect> shaderEffect) {
			if (!IsHandleValid(shaderEffect)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			auto& effect = Get().m_ShaderEffects[shaderEffect];
			return std::cref(effect.shaders);
		}

		std::expected<void, ErrorCode> VulkanShaderEffectManager::ChangeShaderEffectShaderPair(const Core::Handle<ShaderEffect> shaderEffect, Core::AssetRef<VertFragShaderPair> shaderPair) {
			if (!IsHandleValid(shaderEffect)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			if (!shaderPair.IsInitialized()) {
				return std::unexpected(ErrorCode::eUninitializedAssetRef);
			}

			Get().m_ShaderEffects[shaderEffect].shaders = std::move(shaderPair);
			return {};
		}

		std::expected<std::reference_wrapper<const PipelineState>, ErrorCode> VulkanShaderEffectManager::GetShaderEffectPipelineState(const Core::ConstHandle<ShaderEffect> shaderEffect) {
			if (!IsHandleValid(shaderEffect)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			auto& effect = Get().m_ShaderEffects[shaderEffect];
			return std::cref(effect.pipelineState);
		}

		std::expected<void, ErrorCode> VulkanShaderEffectManager::ChangeShaderEffectPipelineState(const Core::Handle<ShaderEffect> shaderEffect, const PipelineState& newPipelineState) {
			if (!IsHandleValid(shaderEffect)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			auto& effect = Get().m_ShaderEffects[shaderEffect];
			effect.pipelineState = newPipelineState;
			return {};
		}

		void VulkanShaderEffectManager::AssignPlaceholder(const Core::Handle<ShaderEffect> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
			CORI_CORE_ASSERT(IsHandleValid(handle), "ShaderEffect handle passed to AssignPlaceholder is invalid.");

			m_ShaderEffects[handle] = m_ShaderEffects[m_PlaceholderEffect];

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> PLACEHOLDER shader effect (copied from handle [%u, %u], placeholder shader pair refd)", handle.GetIndex(), handle.GetVersion(), m_PlaceholderEffect.GetIndex(), m_PlaceholderEffect.GetVersion());
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned PLACEHOLDER shader effect (copied from handle [%u, %u])", CORI_CLEAN_TYPE_NAME(ShaderEffect), handle.GetIndex(), handle.GetVersion(), m_PlaceholderEffect.GetIndex(), m_PlaceholderEffect.GetVersion());
		}
	}
}
