#pragma once
#include "VulkanEngine.hpp"
#include "VulkanShaderManager.hpp"
#include "VulkanTextureManager.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "FileSystem/PathManager.hpp"

namespace Cori {
	namespace Graphics {
		using ShaderEffectIndex = uint32_t;

		struct MaterialData {
			alignas(16) glm::vec4 colorFactor{ 1.0f };
			Core::Handle<Texture2> albedoTexture;
			SamplerHandle albedoSampler{ 0 };
			uint32_t pad;
		};

		struct Material {
			MaterialData customData;
			ShaderEffectIndex shaderEffectIndex{ 0 };
			uint32_t version{ 0 };
			uint32_t pad1;
			uint32_t pad2;
		};

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

		struct ShaderEffect {
			Core::Handle<VertFragShaderPair> shaders;
			PipelineState pipelineState;
			#ifdef DEBUG_BUILD
			std::string name{ "Unnamed shader effect" };
			#endif
		};

		class VulkanMaterialSystem {
		public:
			using OnShaderEffectDeletedFn = std::function<void(const Core::Handle<ShaderEffect>)>;
			using OnShaderEffectSwappedFn = std::function<void(const Core::Handle<Material>, const Core::Handle<ShaderEffect> oldFx, const Core::Handle<ShaderEffect> newFx)>;

			static void Init();

			static void Shutdown();

			static VulkanMaterialSystem& Get();

			[[nodiscard]] static Core::Handle<ShaderEffect> CreateShaderEffect(const Core::Handle<VertFragShaderPair> shaderPair, const PipelineState& state, const ShaderEffectData& data = {}, const char* name = "") {
				if (!VulkanShaderManager::IsHandleValid(shaderPair)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Vert+Frag Shader pair handle passed to CreateShaderEffect, returning placeholder shader effect.");
					return Get().m_PlaceholderEffect;
				}

				Core::Handle<ShaderEffect> handle;

				#ifdef DEBUG_BUILD
				if (strcmp(name, "") != 0) {
					handle = Get().m_ShaderEffects.Emplace(shaderPair, state, name);
				} else {
					handle = Get().m_ShaderEffects.Emplace(shaderPair, state);
				}
				#else
				handle = Get().m_ShaderEffects.Emplace(shaderPair, state);
				#endif

				Get().m_ShaderEffectData.Resize(Get().m_ShaderEffects.Capacity() - 1);

				auto dataRef = Get().m_ShaderEffectData[handle.GetIndex() - 1];
				dataRef = data;

				return handle;
			}

			[[nodiscard]] static Core::Handle<Material> CreateMaterial(const Core::Handle<ShaderEffect> shaderEffect, const MaterialData& data, const char* name = "") {
				if (!IsHandleValid(shaderEffect)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid ShaderEffect handle passed to CreateMaterial, returning placeholder material.");
					return Get().m_PlaceholderMaterial;
				}

				auto handle = Get().m_Materials.Emplace(data, shaderEffect.GetIndex());

				#ifdef DEBUG_BUILD
				Get().m_MaterialNames.resize(Get().m_Materials.Capacity());
				Get().m_MaterialNames[handle.GetIndex()] = name;
				#endif

				return handle;
			}

			[[nodiscard]] static Core::Handle<ShaderEffect> DuplicateShaderEffect(const Core::Handle<ShaderEffect> shaderEffect, const char* name = "") {
				if (!IsHandleValid(shaderEffect)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid ShaderEffect handle passed to DuplicateShaderEffect, returning placeholder shader effect.");
					return Get().m_PlaceholderEffect;
				}

				const auto& data = std::as_const(Get().m_ShaderEffectData)[shaderEffect.GetIndex() - 1];
				auto& effect = Get().m_ShaderEffects[shaderEffect];
				return CreateShaderEffect(effect.shaders, effect.pipelineState, data, name);
			}

			[[nodiscard]] static Core::Handle<Material> DuplicateMaterial(const Core::Handle<Material> material, const char* name = "") {
				if (!IsHandleValid(material)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to DuplicateMaterial, returning placeholder material.");
					return Get().m_PlaceholderMaterial;
				}

				const auto& data = std::as_const(Get().m_Materials)[material];
				auto handle = Get().m_Materials.Emplace(data.customData, data.shaderEffectIndex);

				#ifdef DEBUG_BUILD
				Get().m_MaterialNames.resize(Get().m_Materials.Capacity());
				Get().m_MaterialNames[handle.GetIndex()] = name;
				#endif

				return handle;
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<const ShaderEffectData>, ErrorCode> GetShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::cref(std::as_const(Get().m_ShaderEffectData)[shaderEffect.GetIndex() - 1]);
			}

			static std::expected<void, ErrorCode> ChangeShaderEffectData(const Core::Handle<ShaderEffect> shaderEffect, const ShaderEffectData& data) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto dataRef = Get().m_ShaderEffectData[shaderEffect.GetIndex() - 1];
				dataRef = data;
				return {};
			}

			[[nodiscard]] static std::expected<Core::Handle<VertFragShaderPair>, ErrorCode> GetShaderEffectShaderPair(const Core::Handle<ShaderEffect> shaderEffect) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto& effect = Get().m_ShaderEffects[shaderEffect];
				return effect.shaders;
			}

			static std::expected<void, ErrorCode> ChangeShaderEffectShaderPair(const Core::Handle<ShaderEffect> shaderEffect, const Core::Handle<VertFragShaderPair> shaderPair) {
				if (!IsHandleValid(shaderEffect)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				Get().m_ShaderEffects[shaderEffect].shaders = shaderPair;
				return {};
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<const MaterialData>, ErrorCode> GetMaterialData(const Core::Handle<Material> material) {
				if (!IsHandleValid(material)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				const auto& data = std::as_const(Get().m_Materials)[material];
				return std::cref(data.customData);
			}

			static std::expected<void, ErrorCode> ChangeMaterialData(const Core::Handle<Material> material, const MaterialData& data) {
				if (!IsHandleValid(material)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto dataRef = Get().m_Materials[material];
				dataRef->customData = data;
				return {};
			}

			[[nodiscard]] static std::expected<Core::Handle<ShaderEffect>, ErrorCode> GetMaterialShaderEffect(const Core::Handle<Material> material) {
				if (!IsHandleValid(material)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				const auto& data = std::as_const(Get().m_Materials)[material];

				return Get().m_ShaderEffects.GetIndexHandle(data.shaderEffectIndex);
			}

			static std::expected<void, ErrorCode> ChangeMaterialShaderEffect(const Core::Handle<Material> material, const Core::Handle<ShaderEffect> newShaderEffect) {
				if (!IsHandleValid(material)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto dataRef = Get().m_Materials[material];

				if (!Get().m_OnShaderEffectSwappedListeners.empty()) {
					const Material& constRef = dataRef;
					Core::Handle<ShaderEffect> oldShaderEffect = Get().m_ShaderEffects.GetIndexHandle(constRef.shaderEffectIndex);

					for (auto& func : Get().m_OnShaderEffectSwappedListeners) {
						func(material, newShaderEffect, oldShaderEffect);
					}
				}

				dataRef->shaderEffectIndex = newShaderEffect.GetIndex();

				return {};
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<const PipelineState>, ErrorCode> GetShaderEffectPipelineState(const Core::Handle<ShaderEffect> shaderEffect) {
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

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<ShaderEffect> shaderEffect) {
				return Get().m_ShaderEffects.IsHandleValid(shaderEffect);
			}

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<Material> material) {
				return Get().m_Materials.IsHandleValid(material);
			}

			static void AddOnShaderEffectDeleteListener(OnShaderEffectDeletedFn func) {
				Get().m_OnShaderEffectDeletedListeners.emplace_back(std::move(func));
			}

			static void AddOnShaderEffectSwappedListener(OnShaderEffectSwappedFn func) {
				Get().m_OnShaderEffectSwappedListeners.emplace_back(std::move(func));
			}

			static void ClearOnShaderEffectDeleteListeners() {
				Get().m_OnShaderEffectDeletedListeners.clear();
			}

			static void ClearOnShaderEffectSwappedListeners() {
				Get().m_OnShaderEffectSwappedListeners.clear();
			}

			static void DestroyShaderEffect(const Core::Handle<ShaderEffect> shaderEffect) {
				if (!IsHandleValid(shaderEffect)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid ShaderEffect handle was passed to DestroyMaterial.");
					return;
				}

				if (Get().m_PlaceholderEffect == shaderEffect) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Placeholder ShaderEffect handle was passed to DestroyShaderEffect. Can't destroy the placeholder.");
					return;
				}

				for (auto it = Get().m_Materials.cbegin(); it != Get().m_Materials.cend(); ++it) {
					auto& material = *it;
					if (material.shaderEffectIndex == shaderEffect.GetIndex()) {
						ChangeMaterialShaderEffect(it.GetHandle(), Get().m_PlaceholderEffect); //NOLINT
					}
				}

				Get().m_ShaderEffects.Remove(shaderEffect);

				for (auto& func : Get().m_OnShaderEffectDeletedListeners) {
					func(shaderEffect);
				}
			}

			static void DestroyMaterial(const Core::Handle<Material> material) {
				if (!IsHandleValid(material)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle was passed to DestroyMaterial.");
					return;
				}

				if (Get().m_PlaceholderMaterial == material) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Placeholder Material handle was passed to DestroyMaterial. Can't destroy the placeholder.");
					return;
				}

				Get().m_Materials.Remove(material);

				#ifdef DEBUG_BUILD
				Get().m_MaterialNames[material.GetIndex()].clear();
				#endif
			}

			static void Sync() {
				Get().m_Materials.Sync();
				Get().m_ShaderEffectData.Sync();
			}

			[[nodiscard]] static uint64_t GetMaterialSlotMapBDA() {
				return Get().m_Materials.GetVulkanBuffer().GetBDA();
			}

			[[nodiscard]] static uint64_t GetShaderEffectDataBufferBDA() {
				return Get().m_ShaderEffectData.GetVulkanBuffer().GetBDA();
			}

			~VulkanMaterialSystem() = default;

		private:
			VulkanMaterialSystem() {
				m_Materials.Reserve(512);
				#ifdef DEBUG_BUILD
				m_MaterialNames.reserve(512);
				#endif

				m_ShaderEffects.Reserve(32);
				m_ShaderEffectData.Reserve(32);

				MaterialData placeholderData{
					.albedoTexture = VulkanTextureManager::GetPlaceholder<Texture2>(),
					.albedoSampler = 0
				};

				#ifdef DEBUG_BUILD
				m_PlaceholderEffect = m_ShaderEffects.Emplace(VulkanShaderManager::GetPlaceholder<VertFragShaderPair>(), PipelineState{}, "Placeholder Shader Effect");
				m_PlaceholderMaterial = m_Materials.Emplace(placeholderData, m_PlaceholderEffect.GetIndex());
				m_MaterialNames.emplace_back("Placeholder Material");
				#else
				m_PlaceholderEffect = m_ShaderEffects.Emplace(VulkanShaderManager::GetPlaceholderShaderPair(), PipelineState{});
				m_PlaceholderMaterial = m_Materials.Emplace(placeholderData, m_PlaceholderEffect.GetIndex());
				#endif

				VulkanShaderManager::AddOnVertFragShaderPairDeletedListener(ShaderPairDeleteListener);
			}

			static void ShaderPairDeleteListener(const Core::Handle<VertFragShaderPair> handle) {
				for (auto& effect : Get().m_ShaderEffects) {
					if (effect.shaders == handle) {
						effect.shaders = VulkanShaderManager::GetPlaceholder<VertFragShaderPair>();
					}
				}
			}

			VulkanFlatSlotMap<Material> m_Materials{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Material Slot Map" };
			#ifdef DEBUG_BUILD
			std::vector<std::string> m_MaterialNames;
			#endif

			Core::FlatSlotMap<ShaderEffect> m_ShaderEffects;
			VulkanDynamicVector<ShaderEffectData> m_ShaderEffectData{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Shader Effect Data Buffer" };

			Core::Handle<Material> m_PlaceholderMaterial;
			Core::Handle<ShaderEffect> m_PlaceholderEffect;

			std::vector<OnShaderEffectDeletedFn> m_OnShaderEffectDeletedListeners;
			std::vector<OnShaderEffectSwappedFn> m_OnShaderEffectSwappedListeners;

			static std::unique_ptr<VulkanMaterialSystem> s_Instance;
		};
	}
}