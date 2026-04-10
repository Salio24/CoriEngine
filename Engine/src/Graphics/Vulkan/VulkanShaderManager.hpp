#pragma once
#include "VulkanEngine.hpp"
#include "VulkanLayoutManager.hpp"
#include "Core/ErrorCodes.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "FileSystem/PathManager.hpp"

//TODO: add shader caching

namespace Cori {
	namespace Graphics {
		struct ComputeShader {
			void Bind(vk::CommandBuffer cmb) const {
				cmb.bindShadersEXT(vk::ShaderStageFlagBits::eCompute, m_ComputeShaderObject);
			}

			[[nodiscard]] const char* GetName() const {
				#ifdef DEBUG_BUILD
				return m_ShaderName.c_str();
				#else
				return "Shader Name unavailable in release build";
				#endif
			}

		protected:
			friend class VulkanShaderManager;
			#ifdef DEBUG_BUILD
			std::string m_ShaderName{ "Unnamed Compute Shader Object" };
			#endif
			vk::ShaderEXT m_ComputeShaderObject;
		};

		struct VertFragShaderPair {
			void Bind(vk::CommandBuffer cmb) const {
				cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, m_VertFragPair);
			}

			[[nodiscard]] const char* GetName() const {
				#ifdef DEBUG_BUILD
				return m_ShaderName.c_str();
				#else
				return "Shader name unavailable in release build";
				#endif
			}

		protected:
			friend class VulkanShaderManager;
			#ifdef DEBUG_BUILD
			std::string m_ShaderName{ "Unnamed VertFrag Shader Object" };
			#endif
			std::array<vk::ShaderEXT, 2> m_VertFragPair;
		};

		class VulkanShaderManager {
		public:
			using OnVertFragShaderPairDeletedFn = std::function<void(const Core::Handle<VertFragShaderPair>)>;

			static void Init();

			static void Shutdown();

			static VulkanShaderManager& Get();

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> Load(const Core::AssetID id) {

			}

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> Reload(const Core::AssetID id) {

			}

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static Core::Handle<T> GetPlaceholder() {

			}

			template<typename T> requires std::same_as<ComputeShader, T> || std::same_as<VertFragShaderPair, T>
			static void AssignPlaceholder(const Core::Handle<T> handle) {

			}

			[[nodiscard]] static Core::Handle<VertFragShaderPair> GetPlaceholderShaderPair() {
				return Get().m_PlaceholderShaderPair;
			}

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<ComputeShader> handle) {
				return Get().m_ComputeShaders.IsHandleValid(handle);
			}

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<VertFragShaderPair> handle) {
				return Get().m_PairShaders.IsHandleValid(handle);
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<ComputeShader>, ErrorCode> GetShader(const Core::Handle<ComputeShader> handle) {
				if (!Get().m_ComputeShaders.IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::ref(Get().m_ComputeShaders[handle]);
			}

			[[nodiscard]] static std::expected<std::reference_wrapper<VertFragShaderPair>, ErrorCode> GetShader(const Core::Handle<VertFragShaderPair> handle) {
				if (!Get().m_PairShaders.IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::ref(Get().m_PairShaders[handle]);
			}

			static void DestroyShader(const Core::Handle<ComputeShader> handle) {
				if (!Get().m_ComputeShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Invalid compute shader handle was passed to DestroyShader.");
					return;
				}

				DeletionQueue::PushShaderObject(Get().m_ComputeShaders[handle].m_ComputeShaderObject, VulkanEngine::GetCurrentFrameInFlight());

				Get().m_ComputeShaders.Remove(handle);
			}

			static void DestroyShader(const Core::Handle<VertFragShaderPair> handle) {
				if (!Get().m_PairShaders.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Invalid Vert+Frag Shader pair handle was passed to DestroyShader.");
					return;
				}

				if (Get().m_PlaceholderShaderPair == handle) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Placeholder Vert+Frag Shader pair handle was passed to DestroyShader. Can't destroy the placeholder.");
					return;
				}

				auto& pair = Get().m_PairShaders[handle];

				DeletionQueue::PushShaderObject(pair.m_VertFragPair[0], VulkanEngine::GetCurrentFrameInFlight());
				DeletionQueue::PushShaderObject(pair.m_VertFragPair[1], VulkanEngine::GetCurrentFrameInFlight());

				Get().m_PairShaders.Remove(handle);

				for (auto& func : Get().m_Listeners) {
					func(handle);
				}
			}

			[[nodiscard]] static Core::Handle<VertFragShaderPair> CreateVertexShaderPair(const void* vertexSource, const uint64_t vertexSourceSize, const char* vertexEntryPoint, const void* fragmentSource, const uint64_t fragmentSourceSize, const char* fragmentEntryPoint, const char* shaderName = "") {
				return Get().CreateVertexShaderPairImpl(vertexSource, vertexSourceSize, vertexEntryPoint, fragmentSource, fragmentSourceSize, fragmentEntryPoint, shaderName);
			}

			[[nodiscard]] static Core::Handle<VertFragShaderPair> CreateVertexShaderPair(const void* source, const uint64_t sourceSize, const char* vertexEntryPoint, const char* fragmentEntryPoint, const char* shaderName = "") {
				return CreateVertexShaderPair(source, sourceSize, vertexEntryPoint, source, sourceSize, fragmentEntryPoint, shaderName);
			}

			[[nodiscard]] static Core::Handle<ComputeShader> CreateComputeShader(const void* source, const uint64_t sourceSize, const char* entryPoint, const char* shaderName) {
				vk::ShaderCreateInfoEXT createInfo {
					.stage = vk::ShaderStageFlagBits::eCompute,
					.codeType = vk::ShaderCodeTypeEXT::eSpirv,
					.codeSize = sourceSize,
					.pCode = source,
					.pName = entryPoint,
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
				};

				ComputeShader object;
				object.m_ComputeShaderObject = vk::ShaderEXT{};

				auto result = VulkanEngine::GetLogicalDevice().createShadersEXT(1, &createInfo, nullptr, &object.m_ComputeShaderObject);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Compute '{}' shader creation failed. Error: {}", shaderName, vk::to_string(result));

				#ifdef DEBUG_BUILD
				if (strcmp(shaderName, "") != 0) {
					object.m_ShaderName = shaderName;
				}
				VulkanEngine::SetDebugName(object.m_ComputeShaderObject, std::format("Compute shader '{}'", object.m_ShaderName));
				#endif


				return Get().m_ComputeShaders.Emplace(object);
			}

			static void AddOnVertFragShaderPairDeletedListener(OnVertFragShaderPairDeletedFn func) {
				Get().m_Listeners.emplace_back(std::move(func));
			}

			static void ClearOnVertFragShaderPairDeletedListener() {
				Get().m_Listeners.clear();
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

		private:

			[[nodiscard]] Core::Handle<VertFragShaderPair> CreateVertexShaderPairImpl(const void* vertexSource, const uint64_t vertexSourceSize, const char* vertexEntryPoint, const void* fragmentSource, const uint64_t fragmentSourceSize, const char* fragmentEntryPoint, const char* shaderName = "") {
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

				VertFragShaderPair object;
				object.m_VertFragPair = std::array<vk::ShaderEXT, 2>{};

				auto result = VulkanEngine::GetLogicalDevice().createShadersEXT(2, infos.data(), nullptr, object.m_VertFragPair.data());
				if (result != vk::Result::eSuccess) {
					if (m_PairShaders.IsHandleValid(m_PlaceholderShaderPair)) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Failed to create Vert+Frag Shader pair '{}'. Error: {}. Returning placeholder shader pair.", shaderName, vk::to_string(result));
						return m_PlaceholderShaderPair;
					}

					CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create placeholder Vert+Frag Shader pair. Error: {}. Aborting.", shaderName, vk::to_string(result));
				}

				#ifdef DEBUG_BUILD
				if (strcmp(shaderName, "") != 0) {
					object.m_ShaderName = shaderName;
				}

				VulkanEngine::SetDebugName(object.m_VertFragPair[0], std::format("Vertex shader from Vertex Shader pair '{}'", object.m_ShaderName));
				VulkanEngine::SetDebugName(object.m_VertFragPair[1], std::format("Fragment shader from Vertex Shader pair '{}'", object.m_ShaderName));
				#endif

				return m_PairShaders.Emplace(object);
			}

			VulkanShaderManager() {
				m_ComputeShaders.Reserve(64);
				m_PairShaders.Reserve(64);

				std::ifstream file(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/DefaultShader.spv", std::ios::ate | std::ios::binary);

				CORI_CORE_ASSERT(file.good(), "Failed to open DefaultShader.spv, can't create placeholder shader, aborting.");

				std::vector<Byte> buffer(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
				file.close();

				m_PlaceholderShaderPair = CreateVertexShaderPairImpl(buffer.data(), buffer.size(), "vertMain", buffer.data(), buffer.size(), "fragMain", "Placeholder Vert+Frag Shader Pair");
			}

			Core::Handle<VertFragShaderPair> m_PlaceholderShaderPair;

			Core::FlatSlotMap<ComputeShader> m_ComputeShaders;
			Core::FlatSlotMap<VertFragShaderPair> m_PairShaders;

			std::vector<OnVertFragShaderPairDeletedFn> m_Listeners;

			static std::unique_ptr<VulkanShaderManager> s_Instance;
		};
	}
}
