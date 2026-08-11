#pragma once
#include "VulkanEngine.hpp"
#include "VulkanShaderManager.hpp"
#include "VulkanTextureManager.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "VulkanShaderEffectManager.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/DataStructures/SparseFlatSlotMap.hpp"
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
			Core::AssetRef<ShaderEffect> shaderEffect;
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
					Core::AssetRef<Texture2> albedoTexture{ Core::Internal::EmptyRef };
					std::string albedoSampler;
				} materialData;

				Core::AssetRef<ShaderEffect> shaderEffect{ Core::Internal::EmptyRef };
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

			~VulkanMaterialSystem() = default;

			static void Init();

			static void Shutdown();

			template<typename T> requires std::same_as<Material, T>
			[[nodiscard]] static Core::Handle<Material> AllocateHandle() {
				return Get().m_HandleAllocator.Allocate();
			}

			static void BindAsset(const Core::Handle<Material> handle, const Core::AssetID id, const uint32_t vectorKey);

			static uint32_t BumpGeneration(const Core::Handle<Material> handle);

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<Material> handle);

			[[nodiscard]] static Core::AssetID GetAssetID(const Core::Handle<Material> handle);

			template<typename T> requires std::same_as<Material, T>
			[[nodiscard]] static Core::Handle<Material> GetPlaceholder() {
				return Get().m_PlaceholderMaterial;
			}

			static bool TryAddRef(const Core::Handle<Material> handle);

			static void AddRef(const Core::Handle<Material> handle);

			static void RemoveRef(const Core::Handle<Material> handle);

			static void SetAssetStatus(const Core::Handle<Material> handle, const AssetStatus newStatus);

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<Material> handle);

			[[nodiscard]] static uint32_t GetIdentityVersion(const Core::Handle<Material> handle);

			[[nodiscard]] static std::expected<std::pair<Core::AssetDependencySet, uint32_t>, ErrorCode> TryReadDependencies(const Core::Handle<Material> handle);

			static void PublishIdentity(const Core::Handle<Material> handle);

			static void RegisterAtSlot(const Core::Handle<Material> handle);

			static void Load(const Core::Handle<Material> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<Material> handle);

			static void QueueUnload(const Core::Handle<Material> handle);

			static void AddOnShaderEffectSwappedListener(void* instance, OnShaderEffectSwappedFn func);

			static void RemoveOnShaderEffectSwappedListener(const void* instance);

			static void ClearOnShaderEffectSwappedListeners();

			static void Sync();

			[[nodiscard]] static uint64_t GetMaterialSlotMapBDA();

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = false;

		protected:
			friend class SceneRenderer;

			void CreateMaterial(const Core::Handle<Material> handle, Core::AssetRef<ShaderEffect> shaderEffect, MaterialData data);

			[[nodiscard]] static Core::Handle<Material> DuplicateMaterial(const Core::Handle<Material> handle, const char* name = "");

			[[nodiscard]] static std::expected<std::reference_wrapper<const MaterialData>, ErrorCode> GetMaterialData(const Core::ConstHandle<Material> handle);

			static std::expected<void, ErrorCode> ChangeMaterialData(const Core::Handle<Material> handle, MaterialData&& data);

			[[nodiscard]] static std::expected<std::reference_wrapper<const Core::AssetRef<ShaderEffect>>, ErrorCode> GetMaterialShaderEffect(const Core::ConstHandle<Material> handle);

			static std::expected<void, ErrorCode> ChangeMaterialShaderEffect(const Core::Handle<Material> handle, Core::AssetRef<ShaderEffect> newShaderEffect);

		private:
			VulkanMaterialSystem();

			static VulkanMaterialSystem& Get();

			void AssignPlaceholder(const Core::Handle<Material> handle);

			Core::AssetHandleAllocator<Material> m_HandleAllocator;

			template<typename T> using MaterialGPUStorage = VulkanGPUSyncedSequentialStorage<T, QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Material Slot Map">;
			Core::SparseFlatSlotMap<Material, 0, false, MaterialGPUStorage> m_Materials;

			Core::Handle<Material> m_PlaceholderMaterial;

			std::vector<std::pair<void*, OnShaderEffectSwappedFn>> m_OnShaderEffectSwappedListeners;

			static std::unique_ptr<VulkanMaterialSystem> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Material, Graphics);
	}
}
