#pragma once
#include "VulkanEngine.hpp"
#include "VulkanLayoutManager.hpp"
#include "Core/ErrorCodes.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "FileSystem/PathManager.hpp"
#include "Utility/GlazeUtils.hpp"

//TODO: add shader caching

namespace Cori {
	namespace Graphics {
		class VulkanShaderManager;

		struct ComputeShader : public Core::SecondaryAssetBase {
			using Manager = VulkanShaderManager;

			vk::ShaderEXT m_ComputeShaderObject{ VK_NULL_HANDLE };
			Core::AssetID assetID{ 0 };
			Core::AssetDeletionPolicy deletionPolicy{};
			bool placeholderAssigned{ false };
		};

		struct VertFragShaderPair : public Core::SecondaryAssetBase {
			using Manager = VulkanShaderManager;

			std::array<vk::ShaderEXT, 2> m_VertFragPair{ VK_NULL_HANDLE, VK_NULL_HANDLE };
			Core::AssetID assetID{ 0 };
			Core::AssetDeletionPolicy deletionPolicy{};
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

		public:
			using OnVertFragShaderPairDeletedFn = std::function<void(const Core::Handle<VertFragShaderPair>)>;

			static void Init();

			static void Shutdown();

			static VulkanShaderManager& Get();

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> Load(const Core::AssetID id) {
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				const auto& dir = Core::AssetManager2::GetAssetDir();

				auto assetFilePath = dir / record.path;

				if constexpr (std::same_as<ComputeShader, T>) {
					auto handle = Get().AllocateComputeShaderHandle();

					Get().m_ComputeShaders[handle].assetID = id;
					Get().m_ComputeShaders[handle].deletionPolicy = record.deletionPolicy;
					record.rawHandleIndex = handle.GetIndex();
					record.rawHandleVersion = handle.GetVersion();

					std::string buffer;
					auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
					if (readError != glz::error_code::none) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), compute shader handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::enum_to_string(readError));
						Get().AssignPlaceholder(handle);
						return handle;
					}

					ComputeShaderJsonAssetDataCombined data;
					auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
					if (parseError) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), compute shader handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::format_error(parseError, buffer));
						Get().AssignPlaceholder(handle);
						return handle;
					}

					auto spvFile = assetFilePath.replace_filename(data.AssetData.spv);

					std::ifstream spvData(spvFile, std::ios::ate | std::ios::binary);

					if (!spvData.good()) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), compute shader handle [{},{}], failed to open spv file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), spvFile.string());
						Get().AssignPlaceholder(handle);
						return handle;
					}

					std::vector<Byte> spvBuffer(spvData.tellg());
					spvData.seekg(0, std::ios::beg);
					spvData.read(reinterpret_cast<char*>(spvBuffer.data()), static_cast<std::streamsize>(spvBuffer.size()));
					spvData.close();

					Get().CreateComputeShader(handle, spvBuffer.data(), spvBuffer.size(), data.AssetData.computeEntry.c_str(), "ADD A NAME LATER");
					return handle;
				}
				else {
					auto handle = Get().AllocateShaderPairHandle();
					Get().m_PairShaders[handle].assetID = id;
					Get().m_PairShaders[handle].deletionPolicy = record.deletionPolicy;
					record.rawHandleIndex = handle.GetIndex();
					record.rawHandleVersion = handle.GetVersion();

					std::string buffer;
					auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
					if (readError != glz::error_code::none) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::enum_to_string(readError));
						Get().AssignPlaceholder(handle);
						return handle;
					}

					ShaderPairJsonAssetDataCombined data;
					auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
					if (parseError) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), assetFilePath.string(), glz::format_error(parseError, buffer));
						Get().AssignPlaceholder(handle);
						return handle;
					}

					auto spvFile = assetFilePath.replace_filename(data.AssetData.spv);

					std::ifstream spvData(spvFile, std::ios::ate | std::ios::binary);

					if (!spvData.good()) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to open spv file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), spvFile.string());
						Get().AssignPlaceholder(handle);
						return handle;
					}

					std::vector<Byte> spvBuffer(spvData.tellg());
					spvData.seekg(0, std::ios::beg);
					spvData.read(reinterpret_cast<char*>(spvBuffer.data()), static_cast<std::streamsize>(spvBuffer.size()));
					spvData.close();

					Get().CreateShaderPair(handle, spvBuffer.data(), spvBuffer.size(), data.AssetData.vertexEntry.c_str(), spvBuffer.data(), spvBuffer.size(), data.AssetData.fragmentEntry.c_str(), "ADD A NAME LATER");
					return handle;
				}
			}

			static void Reload(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id) {

			}

			static void Reload(const Core::Handle<ComputeShader> handle, const Core::AssetID id) {

			}

			static void Unload(const Core::Handle<VertFragShaderPair> handle) {
				if (!Get().m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to Unload is invalid, skipping call.");
					return;
				}

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
				record.status = AssetStatus::eUnloaded;
				record.rawHandleIndex = UINT32_MAX;
				record.rawHandleVersion = 0;

				Get().DestroyShader(handle);
				Get().FreeHandle(handle);
			}

			static void Unload(const Core::Handle<ComputeShader> handle) {
				if (!Get().m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to Unload is invalid, skipping call.");
					return;
				}

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
				record.status = AssetStatus::eUnloaded;
				record.rawHandleIndex = UINT32_MAX;
				record.rawHandleVersion = 0;

				Get().DestroyShader(handle);
				Get().FreeHandle(handle);
			}

			static bool ChangeDeletionPolicy(const Core::Handle<ComputeShader> handle, const Core::AssetDeletionPolicy newPolicy) {
				if (!Get().m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to ChangeDeletionPolicy is invalid, skipping call.");
					return false;
				}

				auto& shader = Get().m_ComputeShaders[handle];
				if (shader.deletionPolicy == newPolicy) {
					return false;
				}

				if (shader.deletionPolicy == Core::AssetDeletionPolicy::eKeepAlive) {
					auto refCount = Get().m_ComputeShadersRefCounts[handle.GetIndex()];
					if (refCount == 0) {
						Unload(handle);
						return true;
					}
				}

				shader.deletionPolicy = newPolicy;
				return false;
			}

			static bool ChangeDeletionPolicy(const Core::Handle<VertFragShaderPair> handle, const Core::AssetDeletionPolicy newPolicy) {
				if (!Get().m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to ChangeDeletionPolicy is invalid, skipping call.");
					return false;
				}

				auto& shader = Get().m_PairShaders[handle];
				if (shader.deletionPolicy == newPolicy) {
					return false;
				}

				if (shader.deletionPolicy == Core::AssetDeletionPolicy::eKeepAlive) {
					auto refCount = Get().m_PairShadersRefCounts[handle.GetIndex()];
					if (refCount == 0) {
						Unload(handle);
						return true;
					}
				}

				shader.deletionPolicy = newPolicy;
				return false;
			}

			static Core::AssetID GetAssetID(const Core::Handle<VertFragShaderPair> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Shader pair handle passed to GetAssetID in VulkanShaderManager is invalid.");

				return Get().m_PairShaders[handle].assetID;
			}

			static Core::AssetID GetAssetID(const Core::Handle<ComputeShader> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Compute shader handle passed to GetAssetID in VulkanShaderManager is invalid.");

				return Get().m_ComputeShaders[handle].assetID;
			}

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> GetPlaceholder() {
				if constexpr (std::same_as<ComputeShader, T>) {
					return Get().m_PlaceholderComputeShader;
				}
				else {
					return Get().m_PlaceholderShaderPair;
				}
			}

			static void AddRef(const Core::Handle<ComputeShader> handle) {
				if (!Get().m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to AddRef is invalid, skipping call.");
					return;
				}

				Get().m_ComputeShadersRefCounts[handle.GetIndex()]++;
			}

			static void AddRef(const Core::Handle<VertFragShaderPair> handle) {
				if (!Get().m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to AddRef is invalid, skipping call.");
					return;
				}

				Get().m_PairShadersRefCounts[handle.GetIndex()]++;
			}

			static void RemoveRef(const Core::Handle<ComputeShader> handle) {
				if (!Get().m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to RemoveRef is invalid, skipping call.");
					return;
				}

				auto count = --Get().m_ComputeShadersRefCounts[handle.GetIndex()];
				if (count == 0 && Get().m_ComputeShaders[handle].deletionPolicy == Core::AssetDeletionPolicy::eRefCounted && handle != Get().m_PlaceholderComputeShader) {
					Unload(handle);
				}
			}

			static void RemoveRef(const Core::Handle<VertFragShaderPair> handle) {
				if (!Get().m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to RemoveRef is invalid, skipping call.");
					return;
				}

				auto count = --Get().m_PairShadersRefCounts[handle.GetIndex()];
				if (count == 0 && Get().m_PairShaders[handle].deletionPolicy == Core::AssetDeletionPolicy::eRefCounted && handle != Get().m_PlaceholderShaderPair) {
					Unload(handle);
				}
			}

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<ComputeShader> handle) {
				return Get().m_ComputeShaders.IsHandleValid(handle);
			}

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<VertFragShaderPair> handle) {
				return Get().m_PairShaders.IsHandleValid(handle);
			}

			static void AddOnVertFragShaderPairDeletedListener(OnVertFragShaderPairDeletedFn func) {
				Get().m_Listeners.emplace_back(std::move(func));
			}

			static void ClearOnVertFragShaderPairDeletedListener() {
				Get().m_Listeners.clear();
			}

			static void Bind(const Core::ConstHandle<VertFragShaderPair> handle, vk::CommandBuffer cmb) {
				if (!Get().m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to Bind is invalid, binding placeholder shader pair.");
					cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, Get().m_PairShaders[Get().m_PlaceholderShaderPair].m_VertFragPair);
					return;
				}

				cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, Get().m_PairShaders[handle].m_VertFragPair);
			}

			static void Bind(const Core::ConstHandle<ComputeShader> handle, vk::CommandBuffer cmb) {
				if (!Get().m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to Bind is invalid, binding placeholder (empty) compute shader.");
					cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eCompute }, Get().m_ComputeShaders[Get().m_PlaceholderComputeShader].m_ComputeShaderObject);
					return;
				}

				cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eCompute }, Get().m_ComputeShaders[handle].m_ComputeShaderObject);
			}

			~VulkanShaderManager() {
				for (auto& cs : m_ComputeShaders) {
					DeletionQueue::PushShaderObject(cs.m_ComputeShaderObject, VulkanEngine::GetCurrentFrameInFlight());
				}

				m_ComputeShaders.Clear();

				for (auto& vfp : m_PairShaders) {
					DeletionQueue::PushShaderObject(vfp.m_VertFragPair[0], VulkanEngine::GetCurrentFrameInFlight());
					DeletionQueue::PushShaderObject(vfp.m_VertFragPair[1], VulkanEngine::GetCurrentFrameInFlight());
				}

				m_PairShaders.Clear();
			}

			static constexpr bool EnableHotReload = true;

		private:
			void AssignPlaceholder(const Core::Handle<ComputeShader> handle) {
				if (!m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Compute shader handle passed to AssignPlaceholder is invalid, skipping call.");
					return;
				}

				auto& object = m_ComputeShaders[handle];
				object.m_ComputeShaderObject = m_ComputeShaders[m_PlaceholderComputeShader].m_ComputeShaderObject;
				object.placeholderAssigned = true;
			}

			void AssignPlaceholder(const Core::Handle<VertFragShaderPair> handle) {
				if (!m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Shader pair handle passed to AssignPlaceholder is invalid, skipping call.");
					return;
				}

				auto& object = m_PairShaders[handle];
				object.m_VertFragPair = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair;
				object.placeholderAssigned = true;
			}

			Core::Handle<VertFragShaderPair> AllocateShaderPairHandle() {
				auto handle = m_PairShaders.Emplace();
				if (handle.GetIndex() >= m_PairShadersRefCounts.size()) {
					m_PairShadersRefCounts.resize(m_PairShadersRefCounts.size() * 1.5f);
				}

				return handle;
			}

			Core::Handle<ComputeShader> AllocateComputeShaderHandle() {
				auto handle = m_ComputeShaders.Emplace();
				if (handle.GetIndex() >= m_ComputeShadersRefCounts.size()) {
					m_ComputeShadersRefCounts.resize(m_ComputeShadersRefCounts.size() * 1.5f);
				}

				return handle;
			}

			void CreateComputeShader(const Core::Handle<ComputeShader> handle, const void* source, const uint64_t sourceSize, const char* entryPoint, const char* shaderName = "") {
				if (!m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to CreateComputeShader is invalid, skipping call.");
					return;
				}

				vk::ShaderCreateInfoEXT createInfo {
					.stage = vk::ShaderStageFlagBits::eCompute,
					.codeType = vk::ShaderCodeTypeEXT::eSpirv,
					.codeSize = sourceSize,
					.pCode = source,
					.pName = entryPoint,
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
				};

				auto& object = m_ComputeShaders[handle];
				vk::ShaderEXT shaderObject{};

				auto result = VulkanEngine::GetLogicalDevice().createShadersEXT(1, &createInfo, nullptr, &shaderObject);
				if (result != vk::Result::eSuccess) {
					CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Compute shader '{}' creation failed, this can have catastrophic consequences, likely crash. Error: {}", shaderName, vk::to_string(result));
					VulkanEngine::GetLogicalDevice().destroyShaderEXT(object.m_ComputeShaderObject);

					if (!object.m_ComputeShaderObject) {
						object.m_ComputeShaderObject = m_ComputeShaders[m_PlaceholderComputeShader].m_ComputeShaderObject;
						object.placeholderAssigned = true;
					}
				} else {
					if (object.m_ComputeShaderObject) {
						if (object.m_ComputeShaderObject != m_ComputeShaders[m_PlaceholderComputeShader].m_ComputeShaderObject) {
							DestroyShader(handle);
						}
					}

					object.m_ComputeShaderObject = shaderObject;

					VulkanEngine::SetDebugName(object.m_ComputeShaderObject, std::format("Compute shader '{}'", shaderName));
				}
			}

			void CreateShaderPair(const Core::Handle<VertFragShaderPair> handle, const void* vertexSource, const uint64_t vertexSourceSize, const char* vertexEntryPoint, const void* fragmentSource, const uint64_t fragmentSourceSize, const char* fragmentEntryPoint, const char* shaderName = "") {
				if (!m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Handle passed to CreateShaderPair is invalid, skipping call.");
					return;
				}

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
				if (result != vk::Result::eSuccess) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Failed to create Vert+Frag Shader pair '{}'. Error: {}. Using placeholder.", shaderName, vk::to_string(result));
					VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObjects[0]);
					VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObjects[1]);

					if (!object.m_VertFragPair[0] && !object.m_VertFragPair[1]) {
						object.m_VertFragPair = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair;
						object.placeholderAssigned = true;
					}
				} else {
					if (object.m_VertFragPair[0] && object.m_VertFragPair[1]) {
						auto& placeholderData = m_PairShaders[m_PlaceholderShaderPair];
						if (object.m_VertFragPair[0] != placeholderData.m_VertFragPair[0] && object.m_VertFragPair[1] != placeholderData.m_VertFragPair[1]) {
							DestroyShader(handle);
						}
					}

					object.m_VertFragPair = shaderObjects;

					VulkanEngine::SetDebugName(object.m_VertFragPair[0], std::format("Vertex shader from Vertex Shader pair '{}'", shaderName));
					VulkanEngine::SetDebugName(object.m_VertFragPair[1], std::format("Fragment shader from Vertex Shader pair '{}'", shaderName));
				}
			}

			void DestroyShader(const Core::Handle<ComputeShader> handle) {
				if (!m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Invalid compute shader handle was passed to DestroyShader.");
					return;
				}

				if (m_PlaceholderComputeShader == handle) {
					return;
				}

				auto& object = m_ComputeShaders[handle];

				if (!object.placeholderAssigned) {
					DeletionQueue::PushShaderObject(m_ComputeShaders[handle].m_ComputeShaderObject, VulkanEngine::GetCurrentFrameInFlight());
				}

				object.placeholderAssigned = false;
			}

			void DestroyShader(const Core::Handle<VertFragShaderPair> handle) {
				if (!m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Invalid Vert+Frag Shader pair handle was passed to DestroyShader.");
					return;
				}

				if (m_PlaceholderShaderPair == handle) {
					return;
				}

				auto& pair = m_PairShaders[handle];

				if (!pair.placeholderAssigned) {
					DeletionQueue::PushShaderObject(pair.m_VertFragPair[0], VulkanEngine::GetCurrentFrameInFlight());
					DeletionQueue::PushShaderObject(pair.m_VertFragPair[1], VulkanEngine::GetCurrentFrameInFlight());
				}

				pair.placeholderAssigned = false;
			}

			void FreeHandle(const Core::Handle<ComputeShader> handle) {
				if (!m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Invalid compute shader handle was passed to FreeHandle.");
					return;
				}

				if (m_PlaceholderComputeShader == handle) {
					return;
				}

				m_ComputeShadersRefCounts[handle.GetIndex()] = 0;
				m_ComputeShaders.Remove(handle);
			}

			void FreeHandle(const Core::Handle<VertFragShaderPair> handle) {
				if (!m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Invalid compute shader handle was passed to FreeHandle.");
					return;
				}

				if (m_PlaceholderShaderPair == handle) {
					return;
				}

				m_PairShadersRefCounts[handle.GetIndex()] = 0;
				m_PairShaders.Remove(handle);

				for (auto& func : m_Listeners) {
					func(handle);
				}
			}

			VulkanShaderManager() {
				m_ComputeShaders.Reserve(64);
				m_PairShaders.Reserve(64);
				m_ComputeShadersRefCounts.resize(64);
				m_PairShadersRefCounts.resize(64);

				std::ifstream file(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/DefaultShader.spv", std::ios::ate | std::ios::binary);

				CORI_CORE_ASSERT(file.good(), "Failed to open DefaultShader.spv, can't create placeholder shader, skipping call.");

				std::vector<Byte> buffer(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
				file.close();

				m_PlaceholderShaderPair = AllocateShaderPairHandle();

				CreateShaderPair(m_PlaceholderShaderPair, buffer.data(), buffer.size(), "vertMain", buffer.data(), buffer.size(), "fragMain", "Placeholder Vert+Frag Shader Pair");

				CORI_CORE_ASSERT(m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[0] && m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[1], "Placeholder Vert+Frag shader pair creation failed.");

				std::vector<Byte> buffer2{
					0x03, 0x02, 0x23, 0x07, 0x00, 0x06, 0x01, 0x00, 0x00, 0x00, 0x28, 0x00,
					0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
					0x01, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x05, 0x00, 0x05, 0x00, 0x00, 0x00,
					0x02, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
					0x10, 0x00, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
					0x40, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
					0x03, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
					0x05, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e,
					0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00,
					0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
					0x36, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
					0x04, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00
				};

				m_PlaceholderComputeShader = AllocateComputeShaderHandle();

				CreateComputeShader(m_PlaceholderComputeShader, buffer2.data(), buffer2.size(), "main", "Placeholder Compute Shader");

				CORI_CORE_ASSERT(m_ComputeShaders[m_PlaceholderComputeShader].m_ComputeShaderObject, "Placeholder compute shader creation failed.");
			}

			Core::Handle<ComputeShader> m_PlaceholderComputeShader;
			Core::Handle<VertFragShaderPair> m_PlaceholderShaderPair;

			Core::FlatSlotMap<ComputeShader> m_ComputeShaders;
			Core::FlatSlotMap<VertFragShaderPair> m_PairShaders;

			std::vector<uint32_t> m_ComputeShadersRefCounts;
			std::vector<uint32_t> m_PairShadersRefCounts;

			std::vector<OnVertFragShaderPairDeletedFn> m_Listeners;

			static std::unique_ptr<VulkanShaderManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(VertFragShaderPair, Graphics);
		CORI_ADD_ASSET_TRAITS(ComputeShader, Graphics);
	}
}
