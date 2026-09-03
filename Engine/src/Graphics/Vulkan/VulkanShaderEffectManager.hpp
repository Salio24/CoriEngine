#pragma once
#include "VulkanEngine.hpp"
#include "VulkanShaderManager.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/AssetManager/AssetDependencies.hpp"
#include "VulkanFlagsGlaze.hpp"
#include "Core/DataStructures/SequentialStorage.hpp"
#include "Core/DataStructures/SparseFlatSlotMap.hpp"

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
			bool depthWriteEnable{ false };
			bool depthBoundsTestEnable{ false };
			bool depthBiasEnable{ false };
			bool stencilTestEnable{ false };
			bool logicOpEnable{ false };

			void Change(vk::CommandBuffer& cmb) const {
				cmb.setCullMode(cullMode);
				cmb.setFrontFace(frontFace);
				cmb.setDepthTestEnable(depthTestEnable);
				cmb.setDepthWriteEnable(depthWriteEnable);
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
				static constexpr std::array<std::string_view, 1> RequiredKeys{ "shaderPair" };
				Core::AssetRef<VertFragShaderPair> shaderPair{ Core::Internal::EmptyRef };
				struct PipelineStateGlaze {
					static constexpr std::array<std::string_view, 0> RequiredKeys{};
					Utility::GlazeWithFallback<vk::CullModeFlags, vk::CullModeFlagBits::eNone | vk::CullModeFlagBits::eFront, "cullMode from VulkanShaderEffectManager::Load"> cullMode;
					Utility::GlazeWithFallback<vk::FrontFace, vk::FrontFace::eCounterClockwise, "frontFace from VulkanShaderEffectManager::Load"> frontFace;
					Utility::GlazeWithFallback<vk::CompareOp, vk::CompareOp::eGreater, "depthCompareOp from VulkanShaderEffectManager::Load"> depthCompareOp;
					Utility::GlazeWithFallback<bool, false, "depthTestEnable from VulkanShaderEffectManager::Load"> depthTestEnable;
					Utility::GlazeWithFallback<bool, false, "depthWriteEnable from VulkanShaderEffectManager::Load"> depthWriteEnable;
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

			~VulkanShaderEffectManager() = default;

			static void Init();

			static void Shutdown();

			template<typename T> requires std::same_as<ShaderEffect, T>
			[[nodiscard]] static Core::Handle<ShaderEffect> AllocateHandle() {
				return Get().m_HandleAllocator.Allocate();
			}

			static void BindAsset(const Core::Handle<ShaderEffect> handle, const Core::AssetID id, const uint32_t vectorKey);

			static uint32_t BumpGeneration(const Core::Handle<ShaderEffect> handle);

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<ShaderEffect> handle);

			[[nodiscard]] static Core::AssetID GetAssetID(const Core::Handle<ShaderEffect> handle);

			template<typename T> requires std::same_as<ShaderEffect, T>
			[[nodiscard]] static Core::Handle<ShaderEffect> GetPlaceholder() {
				return Get().m_PlaceholderEffect;
			}

			static bool TryAddRef(const Core::Handle<ShaderEffect> handle);

			static void AddRef(const Core::Handle<ShaderEffect> handle);

			static void RemoveRef(const Core::Handle<ShaderEffect> handle);

			static void SetAssetStatus(const Core::Handle<ShaderEffect> handle, const AssetStatus newStatus);

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<ShaderEffect> handle);

			[[nodiscard]] static uint32_t GetIdentityVersion(const Core::Handle<ShaderEffect> handle);

			[[nodiscard]] static std::expected<std::pair<Core::AssetDependencySet, uint32_t>, ErrorCode> TryReadDependencies(const Core::Handle<ShaderEffect> handle);

			static void PublishIdentity(const Core::Handle<ShaderEffect> handle);

			static void RegisterAtSlot(const Core::Handle<ShaderEffect> handle);

			static void Load(const Core::Handle<ShaderEffect> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<ShaderEffect> handle);

			static void QueueUnload(const Core::Handle<ShaderEffect> handle);

			static void AddOnShaderEffectDeleteListener(void* instance, OnShaderEffectDeletedFn func);

			static void RemoveOnShaderEffectDeleteListener(const void* instance);

			static void ClearOnShaderEffectDeleteListeners();

			static void Sync();

			[[nodiscard]] static uint64_t GetShaderEffectDataBufferBDA();

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = false;

		protected:
			friend class SceneRenderer;

			void CreateShaderEffect(const Core::Handle<ShaderEffect> handle, Core::AssetRef<VertFragShaderPair> shaderPair, const PipelineState& state, const ShaderEffectData& data = {});

			static std::expected<Core::Handle<ShaderEffect>, ErrorCode> DuplicateShaderEffect(const Core::Handle<ShaderEffect> original, std::filesystem::path path, const Core::AssetDeletionPolicy deletionPolicy, std::string name);

			[[nodiscard]] static std::expected<std::reference_wrapper<const ShaderEffectData>, ErrorCode> GetShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect);

			static std::expected<void, ErrorCode> ChangeShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect, const ShaderEffectData& data);

			[[nodiscard]] static std::expected<std::reference_wrapper<const Core::AssetRef<VertFragShaderPair>>, ErrorCode> GetShaderEffectShaderPair(const Core::ConstHandle<ShaderEffect> shaderEffect);

			static std::expected<void, ErrorCode> ChangeShaderEffectShaderPair(const Core::Handle<ShaderEffect> shaderEffect, Core::AssetRef<VertFragShaderPair> shaderPair);

			[[nodiscard]] static std::expected<std::reference_wrapper<const PipelineState>, ErrorCode> GetShaderEffectPipelineState(const Core::ConstHandle<ShaderEffect> shaderEffect);

			static std::expected<void, ErrorCode> ChangeShaderEffectPipelineState(const Core::Handle<ShaderEffect> shaderEffect, const PipelineState& newPipelineState);

		private:
			VulkanShaderEffectManager();

			static VulkanShaderEffectManager& Get();

			void AssignPlaceholder(const Core::Handle<ShaderEffect> handle);

			Core::AssetHandleAllocator<ShaderEffect> m_HandleAllocator;

			Core::SparseFlatSlotMap<ShaderEffect, 0, false> m_ShaderEffects;

			VulkanDynamicVector<ShaderEffectData> m_ShaderEffectData{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Shader Effect Data Buffer" };

			Core::Handle<ShaderEffect> m_PlaceholderEffect;

			std::vector<std::pair<void*, OnShaderEffectDeletedFn>> m_OnShaderEffectDeletedListeners;

			static std::unique_ptr<VulkanShaderEffectManager> s_Instance;
		};
	}
}
