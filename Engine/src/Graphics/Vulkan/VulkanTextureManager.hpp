#pragma once
#include "VulkanEngine.hpp"
#include "VulkanImage.hpp"
#include "Graphics/Image.hpp"
#include "VulkanLayoutManager.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/AssetManager/AssetDependencies.hpp"
#include <backends/imgui_impl_vulkan.h>
#include "VulkanFlagsGlaze.hpp"
#include "Graphics/RenderThreadCommandQueue.hpp"
#include "Core/AssetManager/AssetHandleAllocator.hpp"
#include "Core/DataStructures/SparseFlatSlotMap.hpp"

namespace Cori {
	namespace Graphics {
		using SamplerHandle = uint32_t;
		class VulkanTextureManager;

		struct Texture2 : Core::SecondaryAssetBase {
			using Manager = VulkanTextureManager;
			VulkanImage image;
			vk::ImageView view;
			uint32_t descriptorIndex{ 0 };
			bool placeholderAssigned{ false };
			bool loaded{ false };
		};

		class VulkanTextureManager {
			struct Params {};

			class WorkerPayload {
			public:
				WorkerPayload() = delete;
				WorkerPayload(VulkanImage&& image, std::vector<Byte>&& pixelData) : m_Image(std::move(image)), m_PixelData(std::move(pixelData)) {}

				~WorkerPayload();

				WorkerPayload(const WorkerPayload& other) = delete;
				WorkerPayload& operator=(const WorkerPayload& other) = delete;

				WorkerPayload(WorkerPayload&& other) noexcept;

				WorkerPayload& operator=(WorkerPayload&& other) noexcept;

				void Release();

				VulkanImage m_Image;
				std::vector<Byte> m_PixelData;
				Params m_Params;
			};
			struct JsonAssetData {
				std::string image;
			};

			struct JsonAssetDataCombined {
				glz::skip Metadata;
				JsonAssetData AssetData;
			};

			struct SamplerJsonDef {
				std::string Alias;

				vk::SamplerCreateFlags Flags;
				vk::Filter MagFilter;
				vk::Filter MinFilter;
				vk::SamplerAddressMode AddressModeU;
				vk::SamplerAddressMode AddressModeV;
				vk::SamplerAddressMode AddressModeW;
				float MipLoadBias;
				bool AnisotropyEnable;
				float MaxAnisotropy;
				bool CompareEnable;
				vk::CompareOp CompareOp;
				float MinLod;
				float MaxLod;
				vk::BorderColor BorderColor;
				bool UnnormalizedCoordinates;

				std::optional<uint32_t> internalValue;
			};

			struct GPUAssetTable {
				uint32_t descriptorIndex{ 0 };
				uint32_t version{ 0 };
			};

			struct QueuedUpload {
				VulkanStreamingLine::ImageUpload imageUpload;
				std::vector<Byte> data;
				Core::Handle<Texture2> texture;
				uint32_t loadGen{ 0 };
			};

			struct TextureInTransfer {
				VulkanImage image;
				Core::Handle<Texture2> texture;
				vk::ImageSubresourceLayers subresource;
				uint32_t loadGen{ 0 };
			};

			struct InTransferSlot {
				uint64_t ticket{ 0 };
				std::vector<TextureInTransfer> texturesInTransfer;
			};

			struct SCIHasher {
				std::size_t operator()(const vk::SamplerCreateInfo& info) const noexcept;
			};

			struct TransparentHash {
				using is_transparent = void;

				size_t operator()(std::string_view sv) const noexcept {
					return std::hash<std::string_view>{}(sv);
				}
			};

			struct TransparentEqual {
				using is_transparent = void;

				bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
					return lhs == rhs;
				}
			};

		public:
			~VulkanTextureManager();

			static void Init();

			static void Shutdown();

			template<typename T> requires std::same_as<Texture2, T>
			[[nodiscard]] static Core::Handle<Texture2> AllocateHandle() {
				return Get().m_HandleAllocator.Allocate();
			}

			static void AllocateExtras(const Core::Handle<Texture2> handle);

			static void BindAsset(const Core::Handle<Texture2> handle, const Core::AssetID id, const uint32_t vectorKey);

			static uint32_t BumpGeneration(const Core::Handle<Texture2> handle);

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<Texture2> handle);

			static Core::AssetID GetAssetID(const Core::Handle<Texture2> handle);

			template<typename T> requires std::same_as<Texture2, T>
			static Core::Handle<T> GetPlaceholder() {
				return Get().m_PlaceholderTexture;
			}

			static bool TryAddRef(const Core::Handle<Texture2> handle);

			static void AddRef(const Core::Handle<Texture2> handle);

			static void RemoveRef(const Core::Handle<Texture2> handle);

			static void SetAssetStatus(const Core::Handle<Texture2> handle, const AssetStatus newStatus);

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<Texture2> handle);

			[[nodiscard]] static uint32_t GetIdentityVersion(const Core::Handle<Texture2> handle);

			[[nodiscard]] static std::expected<std::pair<Core::AssetDependencySet, uint32_t>, ErrorCode> TryReadDependencies(const Core::Handle<Texture2> handle);

			static void PublishIdentity(const Core::Handle<Texture2> handle);

			static std::expected<void, ErrorCode> RequestImGuiBinding(const Core::Handle<Texture2> handle);

			[[nodiscard]] static std::optional<ImTextureID> GetImGuiBinding(const Core::Handle<Texture2> handle);

			static void ProcessImGuiBindingRequests();

			static void ReleaseImGuiBinding(const Core::Handle<Texture2> handle);

			static void RegisterAtSlot(const Core::Handle<Texture2> handle);

			static void Load(const Core::Handle<Texture2> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<Texture2> handle);

			static void QueueUnload(const Core::Handle<Texture2> handle);

			[[nodiscard]] static SamplerHandle GetOrCreateSampler(const vk::SamplerCreateInfo& info, const char* alias = "");

			[[nodiscard]] static SamplerHandle GetSampler(const char* alias);

			[[nodiscard]] static bool DoesSamplerExist(const char* alias);

			[[nodiscard]] static bool AssignAliasToSampler(const SamplerHandle handle, const char* alias);

			static void ProcessUpdates(vk::CommandBuffer cmb);

			[[nodiscard]] static uint64_t GetTextureAssetTableBDA();

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = true;
		private:
			VulkanTextureManager();

			static VulkanTextureManager& Get();

			void CreateTexture(const Core::Handle<Texture2> handle, const vk::ImageType type, const vk::Format format, const vk::Extent3D& extent, const uint32_t mipCount, const uint32_t layerCount, const vk::SampleCountFlagBits sampleFlags, const char* name = "");

			bool UpdateTexture(const Core::Handle<Texture2> handle, std::vector<Byte>&& pixels, const vk::Offset3D& offset, const vk::Extent3D& extent, const vk::ImageSubresourceLayers& subresourceLayers, const uint32_t loadGen);

			void DestroyTexture(const Core::Handle<Texture2> handle);

			void ChangeView(const Core::Handle<Texture2> handle, const vk::ImageViewType viewType, const vk::ImageSubresourceRange& subresourceRange);

			void UpdateView(const Core::Handle<Texture2> handle);

			void AssignPlaceholder(const Core::Handle<Texture2> handle);

			void AssignWhitePlaceholder(const Core::Handle<Texture2> handle);

			std::vector<TextureInTransfer>& FindInTransferSlot(const uint64_t value);

			[[nodiscard]] SamplerHandle GetOrCreateSamplerImpl(const vk::SamplerCreateInfo& info, const char* alias = "");

			void LoadSamplers();

			Core::AssetHandleAllocator<Texture2> m_HandleAllocator;

			Core::SparseFlatSlotMap<Texture2, 0, false> m_TexturePool;

			tbb::concurrent_vector<std::atomic<uint64_t>> m_ImGuiDescriptorSets;
			std::mutex m_ImGuiBindingMutex;
			std::vector<Core::Handle<Texture2>> m_PendingImGuiBindings;
			std::vector<Core::Handle<Texture2>> m_ImGuiBindingScratch;

			Core::Handle<Texture2> m_PlaceholderTexture;
			Core::Handle<Texture2> m_WhiteTexture;

			vk::ImageView m_PlaceholderView;
			vk::ImageView m_WhiteView;

			std::vector<uint32_t> m_FreeTextureDescriptorSlots;
			VulkanDynamicVector<GPUAssetTable> m_GPUAssetTables{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Texture Asset Look Up Table" };

			std::array<InTransferSlot, TRANSFERS_IN_FLIGHT + 2> m_TexturesInTransfer;
			std::queue<QueuedUpload> m_QueuedUploads;
			std::vector<vk::ImageMemoryBarrier2> m_BarrierCache;

			std::mutex m_SamplerMutex;
			std::vector<vk::Sampler> m_Samplers;
			std::unordered_map<vk::SamplerCreateInfo, SamplerHandle, SCIHasher> m_SamplerMap;
			std::unordered_map<std::string, SamplerHandle, TransparentHash, TransparentEqual> m_SamplerAliases;

			static constexpr uint32_t s_PlaceholderTextureDescriptorIndex{ 0 };
			static constexpr uint32_t s_WhiteTextureDescriptorIndex{ 1 };
			static constexpr vk::ImageLayout s_DstLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };

			static std::unique_ptr<VulkanTextureManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Texture2, Graphics);
	}
}
