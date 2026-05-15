#pragma once
#include "VulkanEngine.hpp"
#include "VulkanShaderManager.hpp"
#include "VulkanTextureManager.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "VulkanShaderEffectManager.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "FileSystem/PathManager.hpp"

namespace Cori {
	namespace Graphics {
		class VulkanMaterialSystem;

		struct MaterialData {
			alignas(16) glm::vec4 colorFactor{ 1.0f };
			Core::Handle<Texture2> albedoTexture;
			SamplerHandle albedoSampler{ 0 };
			uint32_t pad{ 0 };
		};

		struct Material : Core::SecondaryAssetBase {
			using Manager = VulkanMaterialSystem;
			MaterialData customData;
			Core::AssetRef<ShaderEffect> shaderEffect{};
			uint32_t version{ 0 };
			uint32_t pad1{ 0 };
			uint32_t pad2{ 0 };
		};

		class VulkanMaterialSystem {
			struct MaterialCPUData {
				Core::AssetID assetID;
				Core::AssetDeletionPolicy deletionPolicy;
			};
		public:
			using OnShaderEffectSwappedFn = std::function<void(const Core::Handle<Material>, const Core::ConstHandle<ShaderEffect> oldFx, const Core::ConstHandle<ShaderEffect> newFx)>;

			static void Init();

			static void Shutdown();

			static VulkanMaterialSystem& Get();

			template<typename T> requires std::same_as<Material, T>
			[[nodiscard]] static Core::Handle<T> Load(const Core::AssetID id) {

			}

			static void Unload(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to Unload, skipping call.");
					return;
				}

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
				record.status = AssetStatus::eUnloaded;
				record.rawHandleIndex = UINT32_MAX;
				record.rawHandleVersion = 0;

				Get().DestroyMaterial(handle);
				Get().FreeHandle(handle);
			}

			[[nodiscard]] static Core::AssetID GetAssetID(const Core::Handle<Material> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to GetAssetID in VulkanMaterialManager is invalid.");

				return Get().m_MaterialCPUData[handle.GetIndex()].assetID;
			}

			static void AddRef(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to AddRef, skipping call.");
					return;
				}

				Get().m_RefCounts[handle.GetIndex()]++;
			}

			static void RemoveRef(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to RemoveRef, skipping call.");
					return;
				}

				auto count = --Get().m_RefCounts[handle.GetIndex()];
				if (count == 0 && Get().m_MaterialCPUData[handle.GetIndex()].deletionPolicy == Core::AssetDeletionPolicy::eRefCounted && handle != Get().m_PlaceholderMaterial) {
					Unload(handle);
				}
			}

			template<typename T> requires std::same_as<Material, T>
			[[nodiscard]] static Core::Handle<T> GetPlaceholder() {
				return Get().m_PlaceholderMaterial;
			}

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<Material> material) {
				return Get().m_Materials.IsHandleValid(material);
			}

			static void AddOnShaderEffectSwappedListener(OnShaderEffectSwappedFn func) {
				Get().m_OnShaderEffectSwappedListeners.emplace_back(std::move(func));
			}

			static void ClearOnShaderEffectSwappedListeners() {
				Get().m_OnShaderEffectSwappedListeners.clear();
			}

			static void Sync() {
				Get().m_Materials.Sync();
			}

			[[nodiscard]] static uint64_t GetMaterialSlotMapBDA() {
				return Get().m_Materials.GetVulkanBuffer().GetBDA();
			}

			~VulkanMaterialSystem() = default;

		protected:
			[[nodiscard]] static Core::Handle<Material> DuplicateMaterial(const Core::Handle<Material> material, const char* name = "") {
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

			[[nodiscard]] static std::expected<std::reference_wrapper<const Core::AssetRef<ShaderEffect>>, ErrorCode> GetMaterialShaderEffect(const Core::Handle<Material> material) {
				if (!IsHandleValid(material)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				const auto& data = std::as_const(Get().m_Materials)[material];

				return std::cref(data.shaderEffect);
			}

			static std::expected<void, ErrorCode> ChangeMaterialShaderEffect(const Core::Handle<Material> material, Core::AssetRef<ShaderEffect> newShaderEffect) {
				if (!IsHandleValid(material)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto dataRef = Get().m_Materials[material];

				if (!Get().m_OnShaderEffectSwappedListeners.empty()) {
					const Material& constRef = dataRef;

					for (auto& func : Get().m_OnShaderEffectSwappedListeners) {
						func(material, newShaderEffect.GetHandle(), constRef.shaderEffect.GetHandle());
					}
				}

				dataRef->shaderEffect = std::move(newShaderEffect);

				return {};
			}

		private:
			void AssignPlaceholder(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to AssignPlaceholder, skipping call.");
					return;
				}

				auto dataRef = Get().m_Materials[handle];
				auto& placeholderData = std::as_const(m_Materials)[m_PlaceholderMaterial];

				dataRef->shaderEffect = placeholderData.shaderEffect;
				dataRef->customData = placeholderData.customData;
			}

			[[nodiscard]] Core::Handle<Material> AllocateHandle() {
				auto handle = Get().m_Materials.Emplace();
				if (handle.GetIndex() >= m_RefCounts.size()) {
					m_RefCounts.resize(m_RefCounts.size() * 1.5f);
				}

				if (handle.GetIndex() >= m_MaterialCPUData.size()) {
					m_MaterialCPUData.resize(m_MaterialCPUData.size() * 1.5f);
				}

				return handle;
			}

			void CreateMaterial(const Core::Handle<Material> material, Core::AssetRef<ShaderEffect> shaderEffect, const MaterialData& data) {
				if (!IsHandleValid(material)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle was passed to CreateMaterial.");
					return;
				}

				auto dataRef = m_Materials[material];
				dataRef->shaderEffect = std::move(shaderEffect);
				dataRef->customData = data;
			}

			void DestroyMaterial(const Core::Handle<Material> material) {
				if (!IsHandleValid(material)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle was passed to DestroyMaterial.");
					return;
				}

				if (m_PlaceholderMaterial == material) {
					return;
				}

				//!!!! If i ever wand to add hot reloading support to this, i will need fire the callback in the AssignPlaceholder func as we might change the shader effect there
				//AssignPlaceholder(material);
			}

			void FreeHandle(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to FreeHandle, skipping call.");
					return;
				}

				if (m_PlaceholderMaterial == handle) {
					return;
				}

				m_RefCounts[handle.GetIndex()] = 0;
				m_MaterialCPUData[handle.GetIndex()] = {};

				auto dataRef = m_Materials[handle];
				dataRef->shaderEffect = {};
				dataRef->customData = {};
			}

			VulkanMaterialSystem() {
				m_Materials.Reserve(512);

				MaterialData placeholderData{
					.albedoTexture = VulkanTextureManager::GetPlaceholder<Texture2>(),
					.albedoSampler = 0
				};

				m_PlaceholderMaterial = m_Materials.Emplace(Material{ .customData = placeholderData, .shaderEffect = Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>()) });
			}

			static void ShaderEffectDeletedListener(const Core::Handle<ShaderEffect> handle) {
				for (auto it = Get().m_Materials.cbegin(); it != Get().m_Materials.cend(); ++it) {
					auto& material = *it;
					if (material.shaderEffect.GetHandle() == handle) {
						static_cast<void>(ChangeMaterialShaderEffect(it.GetHandle(), Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>())));
					}
				}
			}

			VulkanFlatSlotMap<Material> m_Materials{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Material Slot Map" };

			std::vector<MaterialCPUData> m_MaterialCPUData;
			std::vector<uint32_t> m_RefCounts;

			Core::Handle<Material> m_PlaceholderMaterial;

			std::vector<OnShaderEffectSwappedFn> m_OnShaderEffectSwappedListeners;

			static std::unique_ptr<VulkanMaterialSystem> s_Instance;
		};
	}
}