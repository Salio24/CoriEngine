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

			struct WorkerPayloadData {
				std::array<float, 4> colorFactor;
				Core::AssetRef<Texture2> albedoTexture;
				std::string albedoSampler;

				Core::AssetRef<ShaderEffect> shaderEffect;
			};

			struct WorkerPayload {
				std::optional<WorkerPayloadData> actualPayload;
			};
		public:
			using OnShaderEffectSwappedFn = std::function<void(void* instance, const Core::Handle<Material> material, const Core::ConstHandle<ShaderEffect> oldFx, const Core::ConstHandle<ShaderEffect> newFx)>;

			static void Init();

			static void Shutdown();

			static VulkanMaterialSystem& Get();

			static void RegisterAtSlot(const Core::Handle<Material> handle) {
				//empty cuz materials are guaranteed to be loaded next frame. Like the compute shaders they are kind of an exception, but for a different reason, to avoid constant rebatching in the scene renderer.
			}

			static void Load(const Core::Handle<Material> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<Material> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
				CORI_CORE_ASSERT(handle != Get().m_PlaceholderMaterial, "Placeholder material handle was passed, can't unload it.")

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

				if (!Get().m_Materials.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				Get().m_Materials.RemoveAt(handle.GetIndex());
			}

			[[nodiscard]] static Core::AssetID GetAssetID(const Core::Handle<Material> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Material handle passed to GetAssetID is invalid.");
				return Get().m_HandleAllocator.GetBoundAssetID(handle);
			}

			static bool TryAddRef(const Core::Handle<Material> handle) {
				return Get().m_HandleAllocator.TryAddRef(handle);
			}

			static void AddRef(const Core::Handle<Material> handle) {
				Get().m_HandleAllocator.AddRef(handle);
			}

			static void RemoveRef(const Core::Handle<Material> handle) {
				Get().m_HandleAllocator.RemoveRef(handle);
			}

			static void BindAsset(const Core::Handle<Material> handle, const Core::AssetID id, const uint32_t vectorKey) {
				return Get().m_HandleAllocator.BindAsset(handle, id, vectorKey);
			}

			static uint32_t BumpGeneration(const Core::Handle<Material> handle) {
				return Get().m_HandleAllocator.BumpGeneration(handle);
			}

			template<typename T> requires std::same_as<Material, T>
			[[nodiscard]] static Core::Handle<Material> GetPlaceholder() {
				return Get().m_PlaceholderMaterial;
			}

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<Material> handle) {
				return Get().m_HandleAllocator.IsHandleValid(handle);
			}

			static void AddOnShaderEffectSwappedListener(void* instance, OnShaderEffectSwappedFn func) {
				Get().m_OnShaderEffectSwappedListeners.emplace_back(instance, std::move(func));
			}

			static void RemoveOnShaderEffectSwappedListener(const void* instance) {
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

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = false;

			void AssignPlaceholder(const Core::Handle<Material> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

				auto& data = m_Materials[handle];
				auto& placeholderData = std::as_const(m_Materials)[m_PlaceholderMaterial];
				data.shaderEffect = placeholderData.shaderEffect;
				data.customData = placeholderData.customData;
			}

			template<typename T> requires std::same_as<Material, T>
			[[nodiscard]] static Core::Handle<Material> AllocateHandle() {
				return Get().m_HandleAllocator.Allocate();
			}

			void CreateMaterial(const Core::Handle<Material> handle, Core::AssetRef<ShaderEffect> shaderEffect, MaterialData data) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

				auto& material = m_Materials[handle];
				material.shaderEffect = std::move(shaderEffect);
				material.customData = std::move(data);
			}

			static void QueueUnload(const Core::Handle<Material> handle) {
				RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
			}

			static void SetAssetStatus(const Core::Handle<Material> handle, const AssetStatus newStatus) {
				Get().m_HandleAllocator.SetAssetStatus(handle, newStatus);
			}

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<Material> handle) {
				return Get().m_HandleAllocator.GetAssetStatus(handle);
			}

		protected:
			friend class SceneRenderer;
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

			[[nodiscard]] static std::expected<std::reference_wrapper<const MaterialData>, ErrorCode> GetMaterialData(const Core::ConstHandle<Material> handle) {
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

			[[nodiscard]] static std::expected<std::reference_wrapper<const Core::AssetRef<ShaderEffect>>, ErrorCode> GetMaterialShaderEffect(const Core::ConstHandle<Material> handle) {
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
					for (auto& [ptr, func] : Get().m_OnShaderEffectSwappedListeners) {
						func(ptr, handle, material.shaderEffect.GetHandle(), newShaderEffect.GetHandle());
					}
				}

				material.shaderEffect = std::move(newShaderEffect);

				return {};
			}
		private:

			VulkanMaterialSystem() {
				m_Materials.Reserve(512);

				m_PlaceholderMaterial = m_HandleAllocator.Allocate();
				m_HandleAllocator.AddRef(m_PlaceholderMaterial);

				MaterialData placeholderData{
					.albedoTexture = Core::AssetRef(VulkanTextureManager::GetPlaceholder<Texture2>()),
					.albedoSampler = 0
				};

				m_Materials.EmplaceAt(m_PlaceholderMaterial.GetIndex(), Material{ .customData = std::move(placeholderData), .shaderEffect = Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>()), .version = m_PlaceholderMaterial.GetVersion() });

				//VulkanShaderEffectManager::AddOnShaderEffectDeleteListener(this, ShaderEffectDeletedListener);
			}

			//static void ShaderEffectDeletedListener([[maybe_unused]] void* instance, const Core::Handle<ShaderEffect> handle) {
			//	for (auto it = Get().m_Materials.cbegin(); it != Get().m_Materials.cend(); ++it) {
			//		auto& material = *it;
			//		if (material.shaderEffect.GetHandle() == handle) {
			//			static_cast<void>(ChangeMaterialShaderEffect(it.GetHandle(), Core::AssetRef(VulkanShaderEffectManager::GetPlaceholder<ShaderEffect>())));
			//		}
			//	}
			//}

			Core::AssetHandleAllocator<Material> m_HandleAllocator;

			VulkanFlatSlotMap<Material, 0, false> m_Materials{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Material Slot Map" };

			Core::Handle<Material> m_PlaceholderMaterial;

			std::vector<std::pair<void*, OnShaderEffectSwappedFn>> m_OnShaderEffectSwappedListeners;

			static std::unique_ptr<VulkanMaterialSystem> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Material, Graphics);
	}
}