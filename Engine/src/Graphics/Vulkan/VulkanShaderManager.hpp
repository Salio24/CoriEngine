#pragma once
#include "VulkanEngine.hpp"
#include "VulkanLayoutManager.hpp"
#include "Core/ErrorCodes.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "FileSystem/PathManager.hpp"
#include "Utility/GlazeUtils.hpp"
#include "Core/AssetManager/AssetHandleAllocator.hpp"
#include "Core/DataStructures/SparseFlatSlotMap.hpp"
#include "Core/Threading/ThreadPool.hpp"
#include "Graphics/RenderThreadCommandQueue.hpp"

//TODO: add shader caching

namespace Cori {
	namespace Graphics {
		class VulkanShaderManager;

		struct ComputeShader : public Core::SecondaryAssetBase {
			static constexpr bool NOPLACEHOLDER{ true };
			using Manager = VulkanShaderManager;

			static constexpr bool EnableAutoHotReload = true;
			vk::ShaderEXT m_ComputeShaderObject{ nullptr };
			bool placeholderAssigned{ false };
		};

		struct VertFragShaderPair : public Core::SecondaryAssetBase {
			using Manager = VulkanShaderManager;

			std::array<vk::ShaderEXT, 2> m_VertFragPair{ nullptr, nullptr };
			bool placeholderAssigned{ false };
		};

		class VulkanShaderManager {
			struct ShaderPairJsonAssetData {
				std::string spv;
				std::string vertexEntry;
				std::string fragmentEntry;
			};

			struct ShaderPairJsonAssetDataCombined {
				glz::skip Metadata;
				ShaderPairJsonAssetData AssetData;
			};

			struct ComputeShaderJsonAssetData {
				std::string spv;
				std::string computeEntry;
			};

			struct ComputeShaderJsonAssetDataCombined {
				glz::skip Metadata;
				ComputeShaderJsonAssetData AssetData;
			};

			class WorkerPayloadPair {
			public:
				WorkerPayloadPair() = delete;
				WorkerPayloadPair(const vk::ShaderEXT vert, const vk::ShaderEXT frag) : m_VertexShader(vert), m_FragmentShader(frag) {}

				~WorkerPayloadPair() {
					if (m_VertexShader) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(m_VertexShader);
					}

					if (m_FragmentShader) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(m_FragmentShader);
					}
				}

				WorkerPayloadPair(const WorkerPayloadPair& other) = delete;
				WorkerPayloadPair& operator=(const WorkerPayloadPair& other) = delete;

				WorkerPayloadPair(WorkerPayloadPair&& other) noexcept {
					m_VertexShader = other.m_VertexShader;
					m_FragmentShader = other.m_FragmentShader;
					other.Release();
				}

				WorkerPayloadPair& operator=(WorkerPayloadPair&& other) noexcept {
					if (m_VertexShader) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(m_VertexShader);
					}

					if (m_FragmentShader) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(m_FragmentShader);
					}

					m_VertexShader = other.m_VertexShader;
					m_FragmentShader = other.m_FragmentShader;
					other.Release();
					return *this;
				}


				void Release() {
					m_VertexShader = nullptr;
					m_FragmentShader = nullptr;
				}

				vk::ShaderEXT m_VertexShader;
				vk::ShaderEXT m_FragmentShader;
			};

			class WorkerPayloadCompute {
			public:
				WorkerPayloadCompute() = delete;
				explicit WorkerPayloadCompute(const vk::ShaderEXT shader) : m_ComputeShader(shader) {}

				~WorkerPayloadCompute() {
					if (m_ComputeShader) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(m_ComputeShader);
					}
				}

				WorkerPayloadCompute(const WorkerPayloadCompute& other) = delete;
				WorkerPayloadCompute& operator=(const WorkerPayloadCompute& other) = delete;

				WorkerPayloadCompute(WorkerPayloadCompute&& other) noexcept {
					m_ComputeShader = other.m_ComputeShader;
					other.Release();
				}

				WorkerPayloadCompute& operator=(WorkerPayloadCompute&& other) noexcept {
					if (m_ComputeShader) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(m_ComputeShader);
					}

					m_ComputeShader = other.m_ComputeShader;
					other.Release();
					return *this;
				}

				void Release() {
					m_ComputeShader = nullptr;
				}

				vk::ShaderEXT m_ComputeShader;
			};

		public:
			using OnVertFragShaderPairDeletedFn = std::function<void(void* instance, const Core::Handle<VertFragShaderPair> handle)>;

			~VulkanShaderManager();

			static void Init();

			static void Shutdown();

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> AllocateHandle() {
				if constexpr (std::same_as<ComputeShader, T>) {
					return Get().m_ComputeShaderHandleAllocator.Allocate();
				}
				else {
					return Get().m_VertFragPairHandleAllocator.Allocate();
				}
			}

			static void BindAsset(const Core::Handle<ComputeShader> handle, const Core::AssetID id, const uint32_t vectorKey);

			static void BindAsset(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id, const uint32_t vectorKey);

			static uint32_t BumpGeneration(const Core::Handle<ComputeShader> handle);

			static uint32_t BumpGeneration(const Core::Handle<VertFragShaderPair> handle);

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<ComputeShader> handle);

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<VertFragShaderPair> handle);

			static Core::AssetID GetAssetID(const Core::Handle<ComputeShader> handle);

			static Core::AssetID GetAssetID(const Core::Handle<VertFragShaderPair> handle);

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> GetPlaceholder() {
				if constexpr (std::same_as<ComputeShader, T>) {
					CORI_CORE_ASSERT(false, "There is no placeholder for Compute Shaders.");
				}
				else {
					return Get().m_PlaceholderShaderPair;
				}
			}

			static bool TryAddRef(const Core::Handle<ComputeShader> handle);

			static bool TryAddRef(const Core::Handle<VertFragShaderPair> handle);

			static void AddRef(const Core::Handle<ComputeShader> handle);

			static void AddRef(const Core::Handle<VertFragShaderPair> handle);

			static void RemoveRef(const Core::Handle<ComputeShader> handle);

			static void RemoveRef(const Core::Handle<VertFragShaderPair> handle);

			static void SetAssetStatus(const Core::Handle<ComputeShader> handle, const AssetStatus newStatus);

			static void SetAssetStatus(const Core::Handle<VertFragShaderPair> handle, const AssetStatus newStatus);

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<ComputeShader> handle);

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<VertFragShaderPair> handle);

			static void RegisterAtSlot([[maybe_unused]] const Core::Handle<ComputeShader> handle);

			static void RegisterAtSlot(const Core::Handle<VertFragShaderPair> handle);

			static void Load(const Core::Handle<ComputeShader> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Load(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<ComputeShader> handle);

			static void Unload(const Core::Handle<VertFragShaderPair> handle);

			static void QueueUnload(const Core::Handle<ComputeShader> handle);

			static void QueueUnload(const Core::Handle<VertFragShaderPair> handle);

			static void Bind(const Core::ConstHandle<ComputeShader> handle, vk::CommandBuffer cmb);

			static void Bind(const Core::ConstHandle<VertFragShaderPair> handle, vk::CommandBuffer cmb);

			static void AddOnVertFragShaderPairDeletedListener(void* instance, OnVertFragShaderPairDeletedFn func);

			static void RemoveOnVertFragShaderPairDeletedListener(const void* instance);

			static void ClearOnVertFragShaderPairDeletedListener();

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = true;

		private:
			VulkanShaderManager();

			static VulkanShaderManager& Get();

			void AssignPlaceholder(const Core::Handle<VertFragShaderPair> handle);

			//leaving it here just for the sake of creating placeholder shader
			void CreateShaderPair(const Core::Handle<VertFragShaderPair> handle, const void* vertexSource, const uint64_t vertexSourceSize, const char* vertexEntryPoint, const void* fragmentSource, const uint64_t fragmentSourceSize, const char* fragmentEntryPoint, const char* shaderName = "");

			void DestroyShader(const Core::Handle<ComputeShader> handle);

			void DestroyShader(const Core::Handle<VertFragShaderPair> handle);

			Core::AssetHandleAllocator<ComputeShader> m_ComputeShaderHandleAllocator;
			Core::AssetHandleAllocator<VertFragShaderPair> m_VertFragPairHandleAllocator;

			vk::ShaderEXT m_PlaceholderVertexShader;
			vk::ShaderEXT m_PlaceholderFragmentShader;

			Core::Handle<VertFragShaderPair> m_PlaceholderShaderPair;

			//Core::FlatSlotMap<ComputeShader, 0, false> m_ComputeShaders;
			//Core::FlatSlotMap<VertFragShaderPair, 0, false> m_PairShaders;

			Core::SparseFlatSlotMap<ComputeShader, 0, false> m_ComputeShaders;

			Core::SparseFlatSlotMap<VertFragShaderPair, 0, false> m_PairShaders;

			std::vector<std::pair<void*, OnVertFragShaderPairDeletedFn>> m_Listeners;

			static std::unique_ptr<VulkanShaderManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(VertFragShaderPair, Graphics);
		CORI_ADD_ASSET_TRAITS(ComputeShader, Graphics);
	}
}
