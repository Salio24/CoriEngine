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
			Core::AssetRef<Texture2> albedoTexture;
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
			uint32_t pad3{ 0 };
			uint32_t pad4{ 0 };
			uint32_t pad5{ 0 };
		};

		class VulkanMaterialSystem {
			struct MaterialCPUData {
				Core::AssetID assetID;
				Core::AssetDeletionPolicy deletionPolicy;
			};

			struct JsonAssetData {
				struct JsonMaterialData {
					std::array<float, 4> colorFactor;
					Core::AssetRef<Texture2> albedoTexture;
					std::string albedoSampler;
				} materialData;

				Core::AssetRef<ShaderEffect> shaderEffect;
			};

			struct JsonAssetDataCombined {
				glz::skip Metadata;
				JsonAssetData AssetData;
			};
		public:
			using OnShaderEffectSwappedFn = std::function<void(const Core::Handle<Material>, const Core::ConstHandle<ShaderEffect> oldFx, const Core::ConstHandle<ShaderEffect> newFx)>;

			static void Init();

			static void Shutdown();

			static VulkanMaterialSystem& Get();

			template<typename T> requires std::same_as<Material, T>
			[[nodiscard]] static Core::Handle<T> Load(const Core::AssetID id) {
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				const auto& dir = Core::AssetManager2::GetAssetDir();
				auto assetFilePath = dir / record.path;

				auto handle = Get().AllocateHandle();
				Get().m_MaterialCPUData[handle.GetIndex()].assetID = id;
				Get().m_MaterialCPUData[handle.GetIndex()].deletionPolicy = record.deletionPolicy;
				record.rawHandleIndex = handle.GetIndex();
				record.rawHandleVersion = handle.GetVersion();

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Load({}), handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::enum_to_string(readError));
					Get().AssignPlaceholder(handle);
					return handle;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Load({}), handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::format_error(parseError, buffer));
					Get().AssignPlaceholder(handle);
					return handle;
				}

				MaterialData materialData {
					.colorFactor = { data.AssetData.materialData.colorFactor[0], data.AssetData.materialData.colorFactor[1], data.AssetData.materialData.colorFactor[2], data.AssetData.materialData.colorFactor[3] },
					.albedoTexture = std::move(data.AssetData.materialData.albedoTexture),
					.albedoSampler = VulkanTextureManager::GetSampler(data.AssetData.materialData.albedoSampler.c_str())
				};

				Get().CreateMaterial(handle, std::move(data.AssetData.shaderEffect), std::move(materialData));
				return handle;
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

			static bool ChangeDeletionPolicy(const Core::Handle<Material> handle, const Core::AssetDeletionPolicy newPolicy) {
				if (!Get().m_Materials.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Handle passed to ChangeDeletionPolicy is invalid, skipping call.");
					return false;
				}

				auto& material = Get().m_MaterialCPUData[handle.GetIndex()];
				if (material.deletionPolicy == newPolicy) {
					return false;
				}

				if (material.deletionPolicy == Core::AssetDeletionPolicy::eKeepAlive) {
					auto refCount = Get().m_RefCounts[handle.GetIndex()];
					if (refCount == 0) {
						Unload(handle);
						return true;
					}
				}

				material.deletionPolicy = newPolicy;
				return false;
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

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<Material> handle) {
				return Get().m_Materials.IsHandleValid(handle);
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

			static constexpr bool EnableHotReload = false;

		protected:
			friend class Renderer;
			[[nodiscard]] static Core::Handle<Material> DuplicateMaterial(const Core::Handle<Material> handle, const char* name = "") {
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

			[[nodiscard]] static std::expected<std::reference_wrapper<const MaterialData>, ErrorCode> GetMaterialData(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				const auto& material = std::as_const(Get().m_Materials)[handle];
				return std::cref(material.customData);
			}

			static std::expected<void, ErrorCode> ChangeMaterialData(const Core::Handle<Material> handle, MaterialData&& data) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				Get().m_Materials[handle].customData = std::move(data);
				return {};
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<const Core::AssetRef<ShaderEffect>>, ErrorCode> GetMaterialShaderEffect(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				const auto& material = std::as_const(Get().m_Materials)[handle];

				return std::cref(material.shaderEffect);
			}

			static std::expected<void, ErrorCode> ChangeMaterialShaderEffect(const Core::Handle<Material> handle, Core::AssetRef<ShaderEffect> newShaderEffect) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				if (!newShaderEffect.IsInitialized()) {
					return std::unexpected(ErrorCode::eUninitializedAssetRef);
				}

				auto& material = Get().m_Materials[handle];

				if (!Get().m_OnShaderEffectSwappedListeners.empty()) {
					for (auto& func : Get().m_OnShaderEffectSwappedListeners) {
						func(handle, newShaderEffect.GetHandle(), material.shaderEffect.GetHandle());
					}
				}

				material.shaderEffect = std::move(newShaderEffect);

				return {};
			}

		private:
			void AssignPlaceholder(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle passed to AssignPlaceholder, skipping call.");
					return;
				}

				auto& data = Get().m_Materials[handle];
				auto& placeholderData = std::as_const(m_Materials)[m_PlaceholderMaterial];
				data.shaderEffect = placeholderData.shaderEffect;
				data.customData = placeholderData.customData;
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

			void CreateMaterial(const Core::Handle<Material> handle, Core::AssetRef<ShaderEffect> shaderEffect, MaterialData data) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle was passed to CreateMaterial, skipping call.");
					return;
				}

				if (!shaderEffect.IsInitialized()) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Uninitialized ShaderEffect AssetRef was passed to CreateMaterial, skipping call.");
					return;
				}

				auto& material = m_Materials[handle];
				material.shaderEffect = std::move(shaderEffect);
				material.customData = std::move(data);
			}

			void DestroyMaterial(const Core::Handle<Material> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MaterialSystem }, "Invalid Material handle was passed to DestroyMaterial, skipping call.");
					return;
				}

				if (m_PlaceholderMaterial == handle) {
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

				auto& material = m_Materials[handle];
				material.shaderEffect = {};
				material.customData = {};
			}

			VulkanMaterialSystem() {
				m_Materials.Reserve(512);
				m_MaterialCPUData.resize(512);
				m_RefCounts.resize(512);

				MaterialData placeholderData{
					.albedoTexture = Core::AssetRef(VulkanTextureManager::GetPlaceholder<Texture2>()),
					.albedoSampler = 0
				};

				m_PlaceholderMaterial = m_Materials.Emplace(Material{ .customData = std::move(placeholderData), .shaderEffect = Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>()) });
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

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Material, Graphics);
	}
}