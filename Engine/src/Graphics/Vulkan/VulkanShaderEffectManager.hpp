#pragma once
#include "VulkanEngine.hpp"
#include "VulkanShaderManager.hpp"
#include "Core/AssetManager/AssetManager2.hpp"

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

		struct ShaderEffect {
			using Manager = VulkanShaderEffectManager;

			Core::AssetRef<VertFragShaderPair> shaders;
			PipelineState pipelineState;
			Core::AssetDeletionPolicy deletionPolicy;
			Core::AssetID assetID;
		};

		class VulkanShaderEffectManager {
		public:
			using OnShaderEffectDeletedFn = std::function<void(const Core::Handle<ShaderEffect>)>;

			static void Init();

			static void Shutdown();

			static VulkanShaderEffectManager& Get();

			template<typename T> requires std::same_as<ShaderEffect, T>
			[[nodiscard]] static Core::Handle<T> Load(const Core::AssetID id) {

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

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<ShaderEffect> handle) {
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
					m_ShaderEffectData.Reserve(m_ShaderEffectData.Size() * 1.5f);
				}

				return handle;
			}

			void CreateShaderEffect(const Core::Handle<ShaderEffect> handle, const Core::Handle<VertFragShaderPair> shaderPair, const PipelineState& state, const ShaderEffectData& data = {}) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid ShaderEffect handle passed to CreateShaderEffect, skipping call.");
					return;
				}

				if (!VulkanShaderManager::IsHandleValid(shaderPair)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Invalid Vert+Frag Shader pair handle passed to CreateShaderEffect, returning placeholder shader effect.");
					return;
				}

				auto& effect = m_ShaderEffects[handle];
				effect.shaders = Core::AssetRef(shaderPair);
				effect.pipelineState = state;

				m_ShaderEffectData.Resize(m_ShaderEffects.Capacity());

				auto dataRef = m_ShaderEffectData[handle.GetIndex()];
				dataRef = data;
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
				m_ShaderEffectData.Reserve(32);

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
}
