#pragma once
#include "VulkanEngine.hpp"
#include "VulkanLayoutManager.hpp"
#include "AssetManager/AssetManager.hpp"
#include "Core/ErrorCodes.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "FileSystem/PathManager.hpp"
#include "Utility/GlazeUtils.hpp"
#include "Core/AssetManager/AssetHandleAllocator.hpp"
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

			static void Init();

			static void Shutdown();

			static VulkanShaderManager& Get();

			static void RegisterAtSlot([[maybe_unused]] const Core::Handle<ComputeShader> handle) {
				//empty cuz compute shaders are guaranteed to be loaded next frame cuz there is no sticky placeholder for them. They are kind of an exception.
			}

			static void RegisterAtSlot(const Core::Handle<VertFragShaderPair> handle);

			static void Load(const Core::Handle<ComputeShader> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Load(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Reload(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id) {

			}

			static void Reload(const Core::Handle<ComputeShader> handle, const Core::AssetID id) {

			}

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<ComputeShader> handle) {
				return Get().m_ComputeShaderHandleAllocator.IsHandleValid(handle);
			}

			[[nodiscard]] static bool IsHandleValid(const Core::ConstHandle<VertFragShaderPair> handle) {
				return Get().m_VertFragPairHandleAllocator.IsHandleValid(handle);
			}

			static void AddOnVertFragShaderPairDeletedListener(void* instance, OnVertFragShaderPairDeletedFn func) {
				Get().m_Listeners.emplace_back(instance, std::move(func));
			}

			static void RemoveOnVertFragShaderPairDeletedListener(const void* instance) {
				std::vector<std::pair<void*, OnVertFragShaderPairDeletedFn>>::iterator result;
				bool isFound = false;
				for (auto it = Get().m_Listeners.begin(); it != Get().m_Listeners.end(); it++) {
					if (it->first == instance) {
						result = it;
						isFound = true;
						break;
					}
				}

				if (isFound) {
					Get().m_Listeners.erase(result);
				}
			}

			static void ClearOnVertFragShaderPairDeletedListener() {
				Get().m_Listeners.clear();
			}

			static void Bind(const Core::ConstHandle<VertFragShaderPair> handle, vk::CommandBuffer cmb) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Bind is invalid.");

				cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, Get().m_PairShaders[handle].m_VertFragPair);
			}

			static void Bind(const Core::ConstHandle<ComputeShader> handle, vk::CommandBuffer cmb) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Bind is invalid.");

				cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eCompute }, Get().m_ComputeShaders[handle].m_ComputeShaderObject);
			}

			~VulkanShaderManager() {
				for (auto& cs : m_ComputeShaders) {
					DeletionQueue::PushShaderObject(cs.m_ComputeShaderObject);
				}

				m_ComputeShaders.Clear();

				for (auto& vfp : m_PairShaders) {
					DeletionQueue::PushShaderObject(vfp.m_VertFragPair[0]);
					DeletionQueue::PushShaderObject(vfp.m_VertFragPair[1]);
				}

				m_PairShaders.Clear();
			}

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = true;

			static void Unload(const Core::Handle<VertFragShaderPair> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
				CORI_CORE_ASSERT(handle != Get().m_PlaceholderShaderPair, "Placeholder shader pair handle was passed, can't unload it.")

				Core::AssetID id = Get().m_VertFragPairHandleAllocator.GetBoundAssetID(handle);
				{
					std::lock_guard lk(Core::AssetManager2::GetMutex());
					auto& record = Core::AssetManager2::GetAssetRecord(id);
					if (record.rawHandleIndex == handle.GetIndex() && record.rawHandleVersion == handle.GetVersion()) {
						record.rawHandleIndex = UINT32_MAX;
						record.rawHandleVersion = 0;
					}
				}

				for (auto& [ptr, func] : Get().m_Listeners) {
					func(ptr, handle);
				}

				Get().m_VertFragPairHandleAllocator.Free(handle);

				if (!Get().m_PairShaders.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				Get().DestroyShader(handle);
				Get().m_PairShaders.RemoveAt(handle.GetIndex());
			}

			static void Unload(const Core::Handle<ComputeShader> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");

				Core::AssetID id = Get().m_ComputeShaderHandleAllocator.GetBoundAssetID(handle);
				{
					std::lock_guard lk(Core::AssetManager2::GetMutex());
					auto& record = Core::AssetManager2::GetAssetRecord(id);
					if (record.rawHandleIndex == handle.GetIndex() && record.rawHandleVersion == handle.GetVersion()) {
						record.rawHandleIndex = UINT32_MAX;
						record.rawHandleVersion = 0;
					}
				}

				Get().m_ComputeShaderHandleAllocator.Free(handle);

				if (!Get().m_ComputeShaders.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				Get().DestroyShader(handle);
				Get().m_ComputeShaders.RemoveAt(handle.GetIndex());
			}

			static void QueueUnload(const Core::Handle<ComputeShader> handle) {
				RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
			}

			static void QueueUnload(const Core::Handle<VertFragShaderPair> handle) {
				RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
			}

			static void AddRef(const Core::Handle<ComputeShader> handle) {
				Get().m_ComputeShaderHandleAllocator.AddRef(handle);
			}

			static void AddRef(const Core::Handle<VertFragShaderPair> handle) {
				Get().m_VertFragPairHandleAllocator.AddRef(handle);
			}

			static void RemoveRef(const Core::Handle<ComputeShader> handle) {
				Get().m_ComputeShaderHandleAllocator.RemoveRef(handle);
			}

			static void RemoveRef(const Core::Handle<VertFragShaderPair> handle) {
				Get().m_VertFragPairHandleAllocator.RemoveRef(handle);
			}

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> GetPlaceholder() {
				if constexpr (std::same_as<ComputeShader, T>) {
					CORI_CORE_ASSERT(false, "There is no placeholder for Compute Shaders.");
				}
				else {
					return Get().m_PlaceholderShaderPair;
				}
			}

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> AllocateHandle() {
				if constexpr (std::same_as<ComputeShader, T>) {
					return Get().m_ComputeShaderHandleAllocator.Allocate();
				}
				else {
					return Get().m_VertFragPairHandleAllocator.Allocate();
				}
			}

			static uint32_t BumpGeneration(const Core::Handle<ComputeShader> handle) {
				return Get().m_ComputeShaderHandleAllocator.BumpGeneration(handle);
			}

			static uint32_t BumpGeneration(const Core::Handle<VertFragShaderPair> handle) {
				return Get().m_VertFragPairHandleAllocator.BumpGeneration(handle);
			}

			static void BindAsset(const Core::Handle<ComputeShader> handle, const Core::AssetID id, const uint32_t vectorKey) {
				return Get().m_ComputeShaderHandleAllocator.BindAsset(handle, id, vectorKey);
			}

			static void BindAsset(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id, const uint32_t vectorKey) {
				return Get().m_VertFragPairHandleAllocator.BindAsset(handle, id, vectorKey);
			}

			static bool TryAddRef(const Core::Handle<ComputeShader> handle) {
				return Get().m_ComputeShaderHandleAllocator.TryAddRef(handle);
			}

			static bool TryAddRef(const Core::Handle<VertFragShaderPair> handle) {
				return Get().m_VertFragPairHandleAllocator.TryAddRef(handle);
			}

			static Core::AssetID GetAssetID(const Core::Handle<ComputeShader> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Compute shader handle passed to GetAssetID is invalid.");
				return Get().m_ComputeShaderHandleAllocator.GetBoundAssetID(handle);
			}

			static Core::AssetID GetAssetID(const Core::Handle<VertFragShaderPair> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Shader pair handle passed to GetAssetID is invalid.");
				return Get().m_VertFragPairHandleAllocator.GetBoundAssetID(handle);
			}

			static void SetAssetStatus(const Core::Handle<ComputeShader> handle, const AssetStatus newStatus) {
				Get().m_ComputeShaderHandleAllocator.SetAssetStatus(handle, newStatus);
			}

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<ComputeShader> handle) {
				return Get().m_ComputeShaderHandleAllocator.GetAssetStatus(handle);
			}

			static void SetAssetStatus(const Core::Handle<VertFragShaderPair> handle, const AssetStatus newStatus) {
				Get().m_VertFragPairHandleAllocator.SetAssetStatus(handle, newStatus);
			}

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<VertFragShaderPair> handle) {
				return Get().m_VertFragPairHandleAllocator.GetAssetStatus(handle);
			}

		private:
			void AssignPlaceholder(const Core::Handle<VertFragShaderPair> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Shader pair handle passed to AssignPlaceholder is invalid.");

				auto& object = m_PairShaders[handle];
				object.m_VertFragPair = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair;
				object.placeholderAssigned = true;
			}

			//leaving it here just for the sake of creating placeholder shader
			void CreateShaderPair(const Core::Handle<VertFragShaderPair> handle, const void* vertexSource, const uint64_t vertexSourceSize, const char* vertexEntryPoint, const void* fragmentSource, const uint64_t fragmentSourceSize, const char* fragmentEntryPoint, const char* shaderName = "") {
				std::array<vk::ShaderCreateInfoEXT, 2> infos;
				infos[0] = {
					.stage = vk::ShaderStageFlagBits::eVertex,
					.nextStage = vk::ShaderStageFlagBits::eFragment,
					.codeType = vk::ShaderCodeTypeEXT::eSpirv,
					.codeSize = vertexSourceSize,
					.pCode = vertexSource,
					.pName = vertexEntryPoint,
					.setLayoutCount = 1,
					.pSetLayouts = &VulkanGlobalLayoutManager::GetGlobalDescriptorSetLayout(),
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
				};

				infos[1] = {
					.stage = vk::ShaderStageFlagBits::eFragment,
					.codeType = vk::ShaderCodeTypeEXT::eSpirv,
					.codeSize = fragmentSourceSize,
					.pCode = fragmentSource,
					.pName = fragmentEntryPoint,
					.setLayoutCount = 1,
					.pSetLayouts = &VulkanGlobalLayoutManager::GetGlobalDescriptorSetLayout(),
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
				};

				auto& object = m_PairShaders[handle];
				std::array<vk::ShaderEXT, 2> shaderObjects{};

				auto result = VulkanEngine::GetLogicalDevice().createShadersEXT(2, infos.data(), nullptr, shaderObjects.data());
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create placeholder shader pair.")

				object.m_VertFragPair = shaderObjects;

				VulkanEngine::SetDebugName(object.m_VertFragPair[0], std::format("Vertex shader from Vertex Shader pair '{}'", shaderName));
				VulkanEngine::SetDebugName(object.m_VertFragPair[1], std::format("Fragment shader from Vertex Shader pair '{}'", shaderName));

			}

			void DestroyShader(const Core::Handle<ComputeShader> handle) {
				auto& object = m_ComputeShaders[handle];

				if (!object.placeholderAssigned) {
					if (m_ComputeShaders[handle].m_ComputeShaderObject != nullptr) {
						DeletionQueue::PushShaderObject(m_ComputeShaders[handle].m_ComputeShaderObject);
					}
				}

				object.placeholderAssigned = false;
			}

			void DestroyShader(const Core::Handle<VertFragShaderPair> handle) {
				if (m_PlaceholderShaderPair == handle) {
					return;
				}

				auto& pair = m_PairShaders[handle];

				if (!pair.placeholderAssigned) {
					if (pair.m_VertFragPair[0] != nullptr) {
						DeletionQueue::PushShaderObject(pair.m_VertFragPair[0]);
					}

					if (pair.m_VertFragPair[1] != nullptr) {
						DeletionQueue::PushShaderObject(pair.m_VertFragPair[1]);
					}
				}

				pair.placeholderAssigned = false;
			}

			VulkanShaderManager() {
				m_ComputeShaders.Reserve(64);
				m_PairShaders.Reserve(64);

				std::ifstream file(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/DefaultShader.spv", std::ios::ate | std::ios::binary);

				CORI_CORE_ASSERT(file.good(), "Failed to open DefaultShader.spv, can't create placeholder shader, skipping call.");

				std::vector<Byte> buffer(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
				file.close();

				m_PlaceholderShaderPair = m_VertFragPairHandleAllocator.Allocate();
				m_VertFragPairHandleAllocator.AddRef(m_PlaceholderShaderPair);

				CreateShaderPair(m_PlaceholderShaderPair, buffer.data(), buffer.size(), "vertMain", buffer.data(), buffer.size(), "fragMain", "Placeholder Vert+Frag Shader Pair");

				CORI_CORE_ASSERT(m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[0] && m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[1], "Placeholder Vert+Frag shader pair creation failed.");
				m_PlaceholderVertexShader = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[0];
				m_PlaceholderFragmentShader = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[1];
			}

			Core::AssetHandleAllocator<ComputeShader> m_ComputeShaderHandleAllocator;
			Core::AssetHandleAllocator<VertFragShaderPair> m_VertFragPairHandleAllocator;

			vk::ShaderEXT m_PlaceholderVertexShader;
			vk::ShaderEXT m_PlaceholderFragmentShader;

			Core::Handle<VertFragShaderPair> m_PlaceholderShaderPair;

			Core::FlatSlotMap<ComputeShader, 0, false> m_ComputeShaders;
			Core::FlatSlotMap<VertFragShaderPair, 0, false> m_PairShaders;

			std::vector<std::pair<void*, OnVertFragShaderPairDeletedFn>> m_Listeners;

			static std::unique_ptr<VulkanShaderManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(VertFragShaderPair, Graphics);
		CORI_ADD_ASSET_TRAITS(ComputeShader, Graphics);
	}
}
