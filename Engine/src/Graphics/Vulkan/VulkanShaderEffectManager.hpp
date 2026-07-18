#pragma once
#include "VulkanEngine.hpp"
#include "VulkanShaderManager.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "VulkanFlagsGlaze.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanShaderEffectManager;

		struct ShaderEffectData {
			alignas(16) glm::vec4 custom1;
			alignas(16) glm::vec4 custom2;
		};

		struct PipelineState {
			vk::CullModeFlags cullMode{ vk::CullModeFlagBits::eNone };
			vk::FrontFace frontFace{ vk::FrontFace::eCounterClockwise };
			vk::CompareOp depthCompareOp{ vk::CompareOp::eGreater };
			bool depthTestEnable{ false };
			bool depthBoundsTestEnable{ false };
			bool depthBiasEnable{ false };
			bool stencilTestEnable{ false };
			bool logicOpEnable{ false };

			void Change(vk::CommandBuffer& cmb) const {
				cmb.setCullMode(cullMode);
				cmb.setFrontFace(frontFace);
				cmb.setDepthTestEnable(depthTestEnable);
				cmb.setDepthCompareOp(depthCompareOp);
				cmb.setDepthBoundsTestEnable(depthBoundsTestEnable);
				cmb.setDepthBiasEnable(depthBiasEnable);
				cmb.setStencilTestEnable(stencilTestEnable);
				cmb.setLogicOpEnableEXT(logicOpEnable);
			}

			bool operator==(const PipelineState& other) const = default;
		};

		struct ShaderEffect : public Core::SecondaryAssetBase {
			using Manager = VulkanShaderEffectManager;

			Core::AssetRef<VertFragShaderPair> shaders;
			PipelineState pipelineState{};
		};
		
		class VulkanShaderEffectManager {
			struct WorkerPayload {
				ShaderEffect effect;
				ShaderEffectData data;
			};

			struct JsonAssetData {
				Core::AssetRef<VertFragShaderPair> shaderPair;
				struct PipelineStateGlaze {
					Utility::GlazeWithFallback<vk::CullModeFlags, vk::CullModeFlagBits::eNone | vk::CullModeFlagBits::eFront, "cullMode from VulkanShaderEffectManager::Load"> cullMode;
					Utility::GlazeWithFallback<vk::FrontFace, vk::FrontFace::eCounterClockwise, "frontFace from VulkanShaderEffectManager::Load"> frontFace;
					Utility::GlazeWithFallback<vk::CompareOp, vk::CompareOp::eGreater, "depthCompareOp from VulkanShaderEffectManager::Load"> depthCompareOp;
					Utility::GlazeWithFallback<bool, false, "depthTestEnable from VulkanShaderEffectManager::Load"> depthTestEnable;
					Utility::GlazeWithFallback<bool, false, "depthBoundsTestEnable from VulkanShaderEffectManager::Load"> depthBoundsTestEnable;
					Utility::GlazeWithFallback<bool, false, "depthBiasEnable from VulkanShaderEffectManager::Load"> depthBiasEnable;
					Utility::GlazeWithFallback<bool, false, "stencilTestEnable from VulkanShaderEffectManager::Load"> stencilTestEnable;
					Utility::GlazeWithFallback<bool, false, "logicOpEnable from VulkanShaderEffectManager::Load"> logicOpEnable;
				} pipelineState;

				struct CustomDataGlaze {
					std::optional<float> custom1;
					std::optional<float> custom2;
					std::optional<float> custom3;
					std::optional<float> custom4;
					std::optional<float> custom5;
					std::optional<float> custom6;
					std::optional<float> custom7;
					std::optional<float> custom8;
				} customData;
			};

			struct JsonAssetDataCombined {
				glz::skip Metadata;
				JsonAssetData AssetData;
			};
		public:
			using OnShaderEffectDeletedFn = std::function<void(void* instance, const Core::Handle<ShaderEffect> handle)>;

			static void Init();

			static void Shutdown();

			static VulkanShaderEffectManager& Get();

			static void RegisterAtSlot(const Core::Handle<ShaderEffect> handle);

			static void Load(const Core::Handle<ShaderEffect> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<ShaderEffect> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
				CORI_CORE_ASSERT(handle != Get().m_PlaceholderEffect, "Placeholder shader effect handle was passed, can't unload it.")

				Core::AssetID id = Get().m_HandleAllocator.GetBoundAssetID(handle);
				{
					std::lock_guard lk(Core::AssetManager2::GetMutex());
					auto& record = Core::AssetManager2::GetAssetRecord(id);
					if (record.rawHandleIndex == handle.GetIndex() && record.rawHandleVersion == handle.GetVersion()) {
						record.rawHandleIndex = UINT32_MAX;
						record.rawHandleVersion = 0;
					}
				}

				Get().m_HandleAllocator.Free(handle);

				if (!Get().m_ShaderEffects.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				Get().m_ShaderEffects.RemoveAt(handle.GetIndex());
			}

			[[nodiscard]] static Core::AssetID GetAssetID(const Core::Handle<ShaderEffect> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "ShaderEffect handle passed to GetAssetID is invalid.");
				return Get().m_HandleAllocator.GetBoundAssetID(handle);
			}

			template<typename T> requires std::same_as<ShaderEffect, T>
			[[nodiscard]] static Core::Handle<ShaderEffect> GetPlaceholder() {
				return Get().m_PlaceholderEffect;
			}

			static uint32_t BumpGeneration(const Core::Handle<ShaderEffect> handle) {
				return Get().m_HandleAllocator.BumpGeneration(handle);
			}

			static void BindAsset(const Core::Handle<ShaderEffect> handle, const Core::AssetID id, const uint32_t vectorKey) {
				return Get().m_HandleAllocator.BindAsset(handle, id, vectorKey);
			}

			static void AddRef(const Core::Handle<ShaderEffect> handle) {
				Get().m_HandleAllocator.AddRef(handle);
			}

			static void RemoveRef(const Core::Handle<ShaderEffect> handle) {
				Get().m_HandleAllocator.RemoveRef(handle);
			}

			static bool TryAddRef(const Core::Handle<ShaderEffect> handle) {
				return Get().m_HandleAllocator.TryAddRef(handle);
			}

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<ShaderEffect> handle) {
				return Get().m_HandleAllocator.IsHandleValid(handle);
			}

			void AssignPlaceholder(const Core::Handle<ShaderEffect> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "ShaderEffect handle passed to AssignPlaceholder is invalid.");

				m_ShaderEffects[handle] = m_ShaderEffects[m_PlaceholderEffect];
			}

			template<typename T> requires std::same_as<ShaderEffect, T>
			[[nodiscard]] static Core::Handle<ShaderEffect> AllocateHandle() {
				return Get().m_HandleAllocator.Allocate();
			}

			void CreateShaderEffect(const Core::Handle<ShaderEffect> handle, Core::AssetRef<VertFragShaderPair> shaderPair, const PipelineState& state, const ShaderEffectData& data = {}) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid ShaderEffect handle passed to CreateShaderEffect");

				auto& effect = m_ShaderEffects[handle];
				effect.shaders = std::move(shaderPair);
				effect.pipelineState = state;

				m_ShaderEffectData[handle.GetIndex()] = data;
			}

			static void QueueUnload(const Core::Handle<ShaderEffect> handle) {
				RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
			}

			static void SetAssetStatus(const Core::Handle<ShaderEffect> handle, const AssetStatus newStatus) {
				Get().m_HandleAllocator.SetAssetStatus(handle, newStatus);
			}

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<ShaderEffect> handle) {
				return Get().m_HandleAllocator.GetAssetStatus(handle);
			}

			static void AddOnShaderEffectDeleteListener(void* instance, OnShaderEffectDeletedFn func) {
				Get().m_OnShaderEffectDeletedListeners.emplace_back(instance, std::move(func));
			}

			static void RemoveOnShaderEffectDeleteListener(const void* instance) {
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

			static void ClearOnShaderEffectDeleteListeners() {
				Get().m_OnShaderEffectDeletedListeners.clear();
			}

			static void Sync() {
				Get().m_ShaderEffectData.Sync();
			}

			[[nodiscard]] static uint64_t GetShaderEffectDataBufferBDA() {
				return Get().m_ShaderEffectData.GetVulkanBuffer().GetBDA();
			}

			~VulkanShaderEffectManager() {
				//VulkanShaderManager::RemoveOnVertFragShaderPairDeletedListener(this);
			}

			static constexpr bool EnableHotReload = false;

		protected:
			friend class SceneRenderer;
			static std::expected<Core::Handle<ShaderEffect>, ErrorCode> DuplicateShaderEffect(const Core::Handle<ShaderEffect> original, std::filesystem::path path, const Core::AssetDeletionPolicy deletionPolicy, std::string name) {
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

			[[nodiscard]] static std::expected<std::reference_wrapper<const ShaderEffectData>, ErrorCode> GetShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::cref(std::as_const(Get().m_ShaderEffectData)[shaderEffect.GetIndex()]);
			}

			static std::expected<void, ErrorCode> ChangeShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect, const ShaderEffectData& data) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				Get().m_ShaderEffectData[shaderEffect.GetIndex()] = data;
				return {};
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<const Core::AssetRef<VertFragShaderPair>>, ErrorCode> GetShaderEffectShaderPair(const Core::ConstHandle<ShaderEffect> shaderEffect) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto& effect = Get().m_ShaderEffects[shaderEffect];
				return std::cref(effect.shaders);
			}

			static std::expected<void, ErrorCode> ChangeShaderEffectShaderPair(const Core::Handle<ShaderEffect> shaderEffect, Core::AssetRef<VertFragShaderPair> shaderPair) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				if (!shaderPair.IsInitialized()) {
					return std::unexpected(ErrorCode::eUninitializedAssetRef);
				}

				Get().m_ShaderEffects[shaderEffect].shaders = std::move(shaderPair);
				return {};
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<const PipelineState>, ErrorCode> GetShaderEffectPipelineState(const Core::ConstHandle<ShaderEffect> shaderEffect) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto& effect = Get().m_ShaderEffects[shaderEffect];
				return std::cref(effect.pipelineState);
			}

			static std::expected<void, ErrorCode> ChangeShaderEffectPipelineState(const Core::Handle<ShaderEffect> shaderEffect, const PipelineState& newPipelineState) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto& effect = Get().m_ShaderEffects[shaderEffect];
				effect.pipelineState = newPipelineState;
				return {};
			}

		private:
			VulkanShaderEffectManager() {
				m_ShaderEffects.Reserve(32);
				m_ShaderEffectData.Resize(32);

				m_PlaceholderEffect = m_HandleAllocator.Allocate();
				m_HandleAllocator.AddRef(m_PlaceholderEffect);

				m_ShaderEffects.EmplaceAt(m_PlaceholderEffect.GetIndex(), ShaderEffect{ .shaders = Core::AssetRef(VulkanShaderManager::GetPlaceholder<VertFragShaderPair>()) });

				//no longer needed due to refcounting
				//VulkanShaderManager::AddOnVertFragShaderPairDeletedListener(this, ShaderPairDeleteListener);
			}

			//static void ShaderPairDeleteListener([[maybe_unused]] void* instance, const Core::Handle<VertFragShaderPair> handle) {
			//	for (auto& effect : Get().m_ShaderEffects) {
			//		if (effect.shaders.GetHandle() == handle) {
			//			effect.shaders = Core::AssetRef(VulkanShaderManager::GetPlaceholder<VertFragShaderPair>());
			//		}
			//	}
			//}

			Core::AssetHandleAllocator<ShaderEffect> m_HandleAllocator;

			Core::FlatSlotMap<ShaderEffect, 0, false> m_ShaderEffects;
			VulkanDynamicVector<ShaderEffectData> m_ShaderEffectData{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Shader Effect Data Buffer" };

			Core::Handle<ShaderEffect> m_PlaceholderEffect;

			std::vector<std::pair<void*, OnShaderEffectDeletedFn>> m_OnShaderEffectDeletedListeners;

			static std::unique_ptr<VulkanShaderEffectManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(ShaderEffect, Graphics);
	}
}
