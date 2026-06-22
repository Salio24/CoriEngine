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
			Core::AssetDeletionPolicy deletionPolicy{};
			Core::AssetID assetID{};
		};

		class VulkanShaderEffectManager {
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
			using OnShaderEffectDeletedFn = std::function<void(const Core::Handle<ShaderEffect>)>;

			static void Init();

			static void Shutdown();

			static VulkanShaderEffectManager& Get();

			template<typename T> requires std::same_as<ShaderEffect, T>
			[[nodiscard]] static Core::Handle<T> Load(const Core::AssetID id) {
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				const auto& dir = Core::AssetManager2::GetAssetDir();
				auto assetFilePath = dir / record.path;

				auto handle = Get().AllocateHandle();
				Get().m_ShaderEffects[handle].assetID = id;
				Get().m_ShaderEffects[handle].deletionPolicy = record.deletionPolicy;
				record.rawHandleIndex = handle.GetIndex();
				record.rawHandleVersion = handle.GetVersion();

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Load({}), handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::enum_to_string(readError));
					Get().AssignPlaceholder(handle);
					return handle;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Load({}), handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::format_error(parseError, buffer));
					Get().AssignPlaceholder(handle);
					return handle;
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
					.depthBoundsTestEnable = data.AssetData.pipelineState.depthBoundsTestEnable,
					.depthBiasEnable = data.AssetData.pipelineState.depthBiasEnable,
					.stencilTestEnable = data.AssetData.pipelineState.stencilTestEnable,
					.logicOpEnable = data.AssetData.pipelineState.logicOpEnable
				};

				Get().CreateShaderEffect(handle, std::move(data.AssetData.shaderPair), state, customData);
				return handle;
			}

			static void Unload(const Core::Handle<ShaderEffect> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle passed to Unload, skipping call.");
					return;
				}

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
				record.status = AssetStatus::eUnloaded;
				record.rawHandleIndex = UINT32_MAX;
				record.rawHandleVersion = 0;

				Get().DestroyShaderEffect(handle);
				Get().FreeHandle(handle);
			}

			static bool ChangeDeletionPolicy(const Core::Handle<ShaderEffect> handle, const Core::AssetDeletionPolicy newPolicy) {
				if (!Get().m_ShaderEffects.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Handle passed to ChangeDeletionPolicy is invalid, skipping call.");
					return false;
				}

				auto& effect = Get().m_ShaderEffects[handle];
				if (effect.deletionPolicy == newPolicy) {
					return false;
				}

				if (effect.deletionPolicy == Core::AssetDeletionPolicy::eKeepAlive) {
					auto refCount = Get().m_RefCounts[handle.GetIndex()];
					if (refCount == 0) {
						Unload(handle);
						return true;
					}
				}

				effect.deletionPolicy = newPolicy;
				return false;
			}

			[[nodiscard]] static Core::AssetID GetAssetID(const Core::Handle<ShaderEffect> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to GetAssetID in VulkanShaderEffectManager is invalid.");

				return Get().m_ShaderEffects[handle].assetID;
			}

			template<typename T> requires std::same_as<ShaderEffect, T>
			[[nodiscard]] static Core::Handle<T> GetPlaceholder() {
				return Get().m_PlaceholderEffect;
			}

			static void AddRef(const Core::Handle<ShaderEffect> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle passed to AddRef, skipping call.");
					return;
				}

				Get().m_RefCounts[handle.GetIndex()]++;
			}

			static void RemoveRef(const Core::Handle<ShaderEffect> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle passed to RemoveRef, skipping call.");
					return;
				}

				auto count = --Get().m_RefCounts[handle.GetIndex()];
				if (count == 0 && Get().m_ShaderEffects[handle].deletionPolicy == Core::AssetDeletionPolicy::eRefCounted && handle != Get().m_PlaceholderEffect) {
					Unload(handle);
				}
			}

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<ShaderEffect> handle) {
				return Get().m_ShaderEffects.IsHandleValid(handle);
			}

			static void AddOnShaderEffectDeleteListener(OnShaderEffectDeletedFn func) {
				Get().m_OnShaderEffectDeletedListeners.emplace_back(std::move(func));
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

			~VulkanShaderEffectManager() = default;

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
			void AssignPlaceholder(const Core::Handle<ShaderEffect> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle passed to AssignPlaceholder, skipping call.");
					return;
				}

				m_ShaderEffects[handle] = m_ShaderEffects[m_PlaceholderEffect];
			}

			[[nodiscard]] Core::Handle<ShaderEffect> AllocateHandle() {
				auto handle = m_ShaderEffects.Emplace();
				if (handle.GetIndex() >= m_RefCounts.size()) {
					m_RefCounts.resize(m_RefCounts.size() * 1.5f);
				}
				if (handle.GetIndex() >= m_ShaderEffectData.Size()) {
					m_ShaderEffectData.Resize(m_ShaderEffectData.Size() * 1.5f);
				}

				return handle;
			}

			void CreateShaderEffect(const Core::Handle<ShaderEffect> handle, Core::AssetRef<VertFragShaderPair> shaderPair, const PipelineState& state, const ShaderEffectData& data = {}) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle passed to CreateShaderEffect, skipping call.");
					return;
				}

				if (!shaderPair.IsInitialized()) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Uninitialized VertFragShaderPair AssetRef was passed to CreateShaderEffect, skipping call.");
					return;
				}

				auto& effect = m_ShaderEffects[handle];
				effect.shaders = std::move(shaderPair);
				effect.pipelineState = state;

				m_ShaderEffectData[handle.GetIndex()] = data;
			}

			void DestroyShaderEffect(const Core::Handle<ShaderEffect> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle was passed to DestroyMaterial.");
					return;
				}

				if (m_PlaceholderEffect == handle) {
					return;
				}

				//for (auto it = Get().m_Materials.cbegin(); it != Get().m_Materials.cend(); ++it) {
				//	auto& material = *it;
				//	if (material.shaderEffectIndex == shaderEffect.GetIndex()) {
				//		ChangeMaterialShaderEffect(it.GetHandle(), Get().m_PlaceholderEffect); //NOLINT
				//	}
				//}

				AssignPlaceholder(handle);
			}

			void FreeHandle(const Core::Handle<ShaderEffect> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle passed to FreeHandle, skipping call.");
					return;
				}

				if (m_PlaceholderEffect == handle) {
					return;
				}

				for (auto& func : m_OnShaderEffectDeletedListeners) {
					func(handle);
				}

				m_RefCounts[handle.GetIndex()] = 0;
				m_ShaderEffects.Remove(handle);
			}

			VulkanShaderEffectManager() {
				m_RefCounts.resize(32);
				m_ShaderEffects.Reserve(32);
				m_ShaderEffectData.Resize(32);

				m_PlaceholderEffect = m_ShaderEffects.Emplace(ShaderEffect{ .shaders = Core::AssetRef(VulkanShaderManager::GetPlaceholder<VertFragShaderPair>()) });

				VulkanShaderManager::AddOnVertFragShaderPairDeletedListener(ShaderPairDeleteListener);
			}

			static void ShaderPairDeleteListener(const Core::Handle<VertFragShaderPair> handle) {
				for (auto& effect : Get().m_ShaderEffects) {
					if (effect.shaders.GetHandle() == handle) {
						effect.shaders = Core::AssetRef(VulkanShaderManager::GetPlaceholder<VertFragShaderPair>());
					}
				}
			}

			std::vector<uint32_t> m_RefCounts;

			Core::FlatSlotMap<ShaderEffect> m_ShaderEffects;
			VulkanDynamicVector<ShaderEffectData> m_ShaderEffectData{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Shader Effect Data Buffer" };

			Core::Handle<ShaderEffect> m_PlaceholderEffect;

			std::vector<OnShaderEffectDeletedFn> m_OnShaderEffectDeletedListeners;

			static std::unique_ptr<VulkanShaderEffectManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(ShaderEffect, Graphics);
	}
}
