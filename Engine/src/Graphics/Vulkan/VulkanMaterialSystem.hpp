#pragma once
#include "VulkanEngine.hpp"
#include "VulkanShaderManager.hpp"
#include "VulkanTextureManager.hpp"
#include "FileSystem/PathManager.hpp"

namespace Cori {
	namespace Graphics {
		using MaterialHandle = uint32_t;
		using ShaderEffectHandle = uint32_t;
		using ShaderEffectSwappedCallbackFn = std::function<void(const MaterialHandle, const ShaderEffectHandle, const ShaderEffectHandle)>;

		struct MaterialData {
			alignas(16) glm::vec4 colorFactor{ 1.0f };
			TextureHandle albedoTexture{ 0 };
			SamplerHandle albedoSampler{ 0 };
			TextureHandle normalTexture{ 0 };
			SamplerHandle normalSampler{ 0 };
			TextureHandle roughnessTexture{ 0 };
			SamplerHandle roughnessSampler{ 0 };
			TextureHandle emissiveTexture{ 0 };
			SamplerHandle emissiveSampler{ 0 };
		};

		struct MaterialCombinedData {
			MaterialData customData;
			ShaderEffectHandle shaderEffectHandle{ 0 };
		};

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

		class VulkanMaterialSystem {
		public:
			static void Init();

			static void Shutdown();

			static VulkanMaterialSystem& Get();

			static bool IsShaderEffectHandleValid(const ShaderEffectHandle handle) {
				return handle < Get().m_ShaderEffectPool.size() && Get().m_ShaderEffectPool[handle].valid;
			}

			static bool IsMaterialHandleValid(const MaterialHandle handle) {
				return handle < Get().m_MaterialPool.size() && Get().m_MaterialPool[handle].valid;
			}

			static void SetShaderEffectSwappedCallback(const ShaderEffectSwappedCallbackFn& callback) {
				Get().m_ShaderEffectSwappedCallback = callback;
			}

			static VulkanBuffer& GetFrameLocalMaterialDataBuffer() {
				return VulkanUploadManager::GetAmazingBuffer(Get().m_MaterialDataBufferHandle).GetCurrentFrameLocalBuffer();
			}

			static VulkanBuffer& GetFrameLocalShaderEffectDataBuffer() {
				return VulkanUploadManager::GetAmazingBuffer(Get().m_ShaderEffectDataBufferHandle).GetCurrentFrameLocalBuffer();
			}

			static uint32_t GetLoadedMaterialCount() {
				return Get().m_NextMaterialHandle;
			}

			static uint32_t GetLoadedShaderEffectCount() {
				return Get().m_NextShaderEffectHandle;
			}

			static ShaderEffectHandle CreateShaderEffect(const ShaderObjectHandle shaderHandle, const ShaderEffectData& initialData, const PipelineState& initialState, const char* name = "") {
				CORI_CORE_ASSERT(VulkanShaderManager::GetShaderObject(shaderHandle).GetType() == ShaderObject::Type::VertexFragmentPair, "Can't use compute shader object in ShaderEffect.");

				ShaderEffectHandle freeHandle;
				if (!Get().m_ShaderEffectHoles.empty()) {
					freeHandle = Get().m_ShaderEffectHoles.back();
					Get().m_ShaderEffectHoles.pop_back();
				} else {
					freeHandle = Get().m_NextShaderEffectHandle++;
					CORI_CORE_ASSERT(freeHandle < MAX_SHADER_EFFECTS - 1, "VulkanMaterialSystem out of shader effect slots.");
				}

				bool named = strcmp(name, "") != 0;

				auto& effect = Get().m_ShaderEffectPool[freeHandle];
				effect.data = initialData;
				effect.pipelineState = initialState;
				effect.shaderObjectHandle = shaderHandle;
				effect.valid = true;
				if (named) {
					effect.name = name;
				}

				AmazingBuffer::UpdateData patch {
					.offset = freeHandle * sizeof(ShaderEffectData),
					.alignment = alignof(ShaderEffectData),
					.data = std::vector<Byte>(sizeof(ShaderEffectData))
				};

				memcpy(patch.data.data(), &initialData, sizeof(ShaderEffectData));

				auto& shaderEffectDataBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_ShaderEffectDataBufferHandle);
				shaderEffectDataBuffer.SubmitUpdate(std::move(patch));

				return freeHandle;
			}

			static ShaderEffectHandle DuplicateShaderEffect(const ShaderEffectHandle handle, const char* name = "") {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid ShaderEffectHandle was passed to VulkanMaterialSystem::DuplicateShaderEffect.");
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				CORI_CORE_ASSERT(shaderEffect.valid, "Shader Effect at handle '{}' is invalid.", handle);

				return CreateShaderEffect(shaderEffect.shaderObjectHandle, shaderEffect.data, shaderEffect.pipelineState, name);
			}

			static ShaderEffectData& GetShaderEffectData(const ShaderEffectHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid ShaderEffectHandle was passed to VulkanMaterialSystem::GetShaderEffectData.");
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				CORI_CORE_ASSERT(shaderEffect.valid, "Shader Effect at handle '{}' is invalid.", handle);

				return shaderEffect.data;
			}

			static void PatchShaderEffectData(const ShaderEffectHandle handle, const ShaderEffectData& data) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid ShaderEffectHandle was passed to VulkanMaterialSystem::PatchShaderEffectData.");
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				CORI_CORE_ASSERT(shaderEffect.valid, "Shader Effect at handle '{}' is invalid.", handle);
				if (CORI_CORE_CHECK(handle != 0, "Shader Effect slot 0 is reserved for default/placeholder shader effect, and thus is immutable, you can't use VulkanMaterialSystem::PatchShaderEffectData on it.")) {
					return;
				}

				AmazingBuffer::UpdateData patch {
					.offset = handle * sizeof(ShaderEffectData),
					.alignment = alignof(ShaderEffectData),
					.data = std::vector<Byte>(sizeof(ShaderEffectData))
				};

				memcpy(patch.data.data(), &data, sizeof(ShaderEffectData));

				auto& shaderEffectDataBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_ShaderEffectDataBufferHandle);
				shaderEffectDataBuffer.SubmitUpdate(std::move(patch));
				shaderEffect.data = data;
			}

			static ShaderObjectHandle GetShaderEffectShaderObject(const ShaderEffectHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid ShaderEffectHandle was passed to VulkanMaterialSystem::GetShaderEffectShaderObject.");
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				CORI_CORE_ASSERT(shaderEffect.valid, "Shader Effect at handle '{}' is invalid.", handle);

				return shaderEffect.shaderObjectHandle;
			}

			static void PatchShaderEffectShaderObject(const ShaderEffectHandle handle, const ShaderObjectHandle newShaderObjectHandle) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid ShaderEffectHandle was passed to VulkanMaterialSystem::PatchShaderEffectShaderObject.");
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				CORI_CORE_ASSERT(shaderEffect.valid, "Shader Effect at handle '{}' is invalid.", handle);
				if (CORI_CORE_CHECK(handle != 0, "Shader Effect slot 0 is reserved for default/placeholder shader effect, and thus is immutable, you can't use VulkanMaterialSystem::PatchShaderEffectShaderObject on it.")) {
					return;
				}
				CORI_CORE_ASSERT(VulkanShaderManager::IsValid(newShaderObjectHandle), "Shader object handle provided to VulkanMaterialSystem::PatchShaderEffectShaderObject is invalid.");
				CORI_CORE_ASSERT(VulkanShaderManager::GetShaderObject(newShaderObjectHandle).GetType() == ShaderObject::Type::VertexFragmentPair, "Can't use compute shader object in ShaderEffect.");

				shaderEffect.shaderObjectHandle = newShaderObjectHandle;
			}

			static PipelineState& GetShaderEffectPipelineState(const ShaderEffectHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid ShaderEffectHandle was passed to VulkanMaterialSystem::GetShaderEffectPipelineState.");
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				CORI_CORE_ASSERT(shaderEffect.valid, "Shader Effect at handle '{}' is invalid.", handle);

				return shaderEffect.pipelineState;
			}

			static void ChangePipelineStateOfShaderEffect(const ShaderEffectHandle handle, const PipelineState& newPipelineState) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid ShaderEffectHandle was passed to VulkanMaterialSystem::ChangePipelineStateOfShaderEffect.");
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				CORI_CORE_ASSERT(shaderEffect.valid, "Shader Effect at handle '{}' is invalid.", handle);
				if (CORI_CORE_CHECK(handle != 0, "Shader Effect slot 0 is reserved for default/placeholder shader effect, and thus is immutable, you can't use VulkanMaterialSystem::ChangePipelineStateOfShaderEffect on it.")) {
					return;
				}

				shaderEffect.pipelineState = newPipelineState;
			}

			static void DestroyShaderEffect(const ShaderEffectHandle handle) {
				// need a fallback/placeholder shader effect
			}

			static MaterialHandle CreateMaterial(const ShaderEffectHandle handle, const MaterialData& initialData, const char* name = "") {
				MaterialHandle freeHandle;
				if (!Get().m_MaterialHoles.empty()) {
					freeHandle = Get().m_MaterialHoles.back();
					Get().m_MaterialHoles.pop_back();
				} else {
					freeHandle = Get().m_NextMaterialHandle++;
					CORI_CORE_ASSERT(freeHandle < MAX_MATERIALS - 1, "VulkanMaterialSystem out of material slots.");
				}

				bool named = strcmp(name, "") != 0;

				auto& material = Get().m_MaterialPool[freeHandle];
				material.data.shaderEffectHandle = handle;
				material.data.customData = initialData;
				material.valid = true;
				if (named) {
					material.name = name;
				}

				AmazingBuffer::UpdateData patch {
					.offset = freeHandle * sizeof(MaterialCombinedData),
					.alignment = alignof(MaterialCombinedData),
					.data = std::vector<Byte>(sizeof(MaterialCombinedData))
				};

				memcpy(patch.data.data(), &material.data, sizeof(MaterialCombinedData));

				auto& materialDataBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_MaterialDataBufferHandle);
				materialDataBuffer.SubmitUpdate(std::move(patch));

				return freeHandle;
			}

			static MaterialHandle DuplicateMaterial(const MaterialHandle handle, const char* name = "") {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid MaterialHandle was passed to VulkanMaterialSystem::DuplicateMaterial.");
				auto& material = Get().m_MaterialPool[handle];
				CORI_CORE_ASSERT(material.valid, "Material at handle '{}' is invalid.", handle);

				return CreateMaterial(handle, material.data.customData, name);
			}

			static MaterialData& GetMaterialData(const MaterialHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid MaterialHandle was passed to VulkanMaterialSystem::GetMaterialData.");
				auto& material = Get().m_MaterialPool[handle];
				CORI_CORE_ASSERT(material.valid, "Material at handle '{}' is invalid.", handle);

				return material.data.customData;
			}

			static void PatchMaterialData(const MaterialHandle handle, const MaterialData& data) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid MaterialHandle was passed to VulkanMaterialSystem::PatchMaterialData.");
				auto& material = Get().m_MaterialPool[handle];
				CORI_CORE_ASSERT(material.valid, "Material at handle '{}' is invalid.", handle);
				if (CORI_CORE_CHECK(handle != 0, "Material slot 0 is reserved for default/placeholder material, and thus is immutable, you can't use VulkanMaterialSystem::PatchMaterialData on it.")) {
					return;
				}

				AmazingBuffer::UpdateData patch {
					.offset = handle * sizeof(MaterialCombinedData),
					.alignment = alignof(MaterialCombinedData),
					.data = std::vector<Byte>(sizeof(MaterialData))
				};

				memcpy(patch.data.data(), &data, sizeof(MaterialData));

				auto& materialDataBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_MaterialDataBufferHandle);
				materialDataBuffer.SubmitUpdate(std::move(patch));
				material.data.customData = data;
			}

			static ShaderEffectHandle GetMaterialShaderEffect(const MaterialHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid MaterialHandle was passed to VulkanMaterialSystem::GetMaterialShaderEffect.");
				auto& material = Get().m_MaterialPool[handle];
				CORI_CORE_ASSERT(material.valid, "Material at handle '{}' is invalid.", handle);

				return material.data.shaderEffectHandle;
			}

			static void PatchMaterialShaderEffect(const MaterialHandle handle, const ShaderEffectHandle newShaderEffectHandle) {
				CORI_CORE_ASSERT(handle < Get().m_ShaderEffectPool.size(), "Invalid MaterialHandle was passed to VulkanMaterialSystem::PatchMaterialShaderEffect.");
				auto& material = Get().m_MaterialPool[handle];
				CORI_CORE_ASSERT(material.valid, "Material at handle '{}' is invalid.", handle);
				CORI_CORE_ASSERT(IsShaderEffectHandleValid(newShaderEffectHandle), "Shader effect at handle '{}' is invalid.", handle);
				if (CORI_CORE_CHECK(handle != 0, "Material slot 0 is reserved for default/placeholder material, and thus is immutable, you can't use VulkanMaterialSystem::PatchMaterialShaderEffect on it.")) {
					return;
				}

				ShaderEffectHandle oldEffectHandle = material.data.shaderEffectHandle;

				AmazingBuffer::UpdateData patch {
					.offset = handle * sizeof(MaterialCombinedData) + sizeof(MaterialData),
					.alignment = alignof(MaterialCombinedData),
					.data = std::vector<Byte>(sizeof(ShaderEffectHandle))
				};

				memcpy(patch.data.data(), &newShaderEffectHandle, sizeof(ShaderEffectHandle));

				auto& materialDataBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_MaterialDataBufferHandle);
				materialDataBuffer.SubmitUpdate(std::move(patch));

				if (Get().m_ShaderEffectSwappedCallback) {
					Get().m_ShaderEffectSwappedCallback(handle, oldEffectHandle, newShaderEffectHandle);
				}

				material.data.shaderEffectHandle = newShaderEffectHandle;
			}

			static void DestroyMaterial(const MaterialHandle handle) {

			}

			static void BindShaderEffect(const ShaderEffectHandle handle, PipelineState& currentState, vk::CommandBuffer cmb) {
				auto& shaderEffect = Get().m_ShaderEffectPool[handle];
				VulkanShaderManager::GetShaderObject(shaderEffect.shaderObjectHandle).Bind(cmb);

				if (shaderEffect.pipelineState != currentState) {
					shaderEffect.pipelineState.Change(cmb);
				}

				currentState = shaderEffect.pipelineState;
			}

			static void ProcessDestructionQueue() {

			}

			~VulkanMaterialSystem() {

			}

		private:
			VulkanMaterialSystem() {
				m_MaterialPool.resize(MAX_MATERIALS);
				m_ShaderEffectPool.resize(MAX_SHADER_EFFECTS);
				uint32_t graphicsQueueFamilyIndex = VulkanEngine::GetGraphicsQueueFamilyIndex();

				AmazingBuffer::CreateInfo materialInfo{
					.size = MAX_MATERIALS * sizeof(MaterialCombinedData),
					.createZeroed = true,
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
					.queueFamilyIndices = { graphicsQueueFamilyIndex },
					.name = "MaterialSystem material data buffer."
				};

				AmazingBuffer::CreateInfo shaderEffectInfo{
					.size = MAX_SHADER_EFFECTS * sizeof(ShaderEffectData),
					.createZeroed = true,
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
					.queueFamilyIndices = { graphicsQueueFamilyIndex },
					.name = "MaterialSystem shader effect data buffer."
				};

				m_MaterialDataBufferHandle = VulkanUploadManager::CreateAmazingBuffer(materialInfo);
				m_ShaderEffectDataBufferHandle = VulkanUploadManager::CreateAmazingBuffer(shaderEffectInfo);

				std::ifstream file(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/DefaultShader.spv", std::ios::ate | std::ios::binary);
				if (!file.is_open()) {
					throw std::runtime_error("failed to open file!");
				}

				std::vector<Byte> buffer(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
				file.close();

				auto shader = VulkanShaderManager::CreateVertexShaderPair(buffer.data(), buffer.size(), "vertMain", "fragMain", "Default/Placeholder Shader");

				ShaderEffectData shaderEffectData{};
				auto& effect = m_ShaderEffectPool[0];
				effect.data = shaderEffectData;
				effect.pipelineState = {};
				effect.shaderObjectHandle = shader;
				effect.valid = true;
				effect.name = "Default/Placeholder Shader Effect";

				AmazingBuffer::UpdateData patch {
					.offset = 0,
					.alignment = alignof(ShaderEffectData),
					.data = std::vector<Byte>(sizeof(ShaderEffectData))
				};

				memcpy(patch.data.data(), &shaderEffectData, sizeof(ShaderEffectData));

				auto& shaderEffectDataBuffer = VulkanUploadManager::GetAmazingBuffer(m_ShaderEffectDataBufferHandle);
				shaderEffectDataBuffer.SubmitUpdate(std::move(patch));

				MaterialCombinedData materialData{};
				auto& material = m_MaterialPool[0];
				material.data.shaderEffectHandle = 0;
				material.data = materialData;
				material.valid = true;
				material.name = "Default/Placeholder Material";

				AmazingBuffer::UpdateData patch_ {
					.offset = 0,
					.alignment = alignof(MaterialCombinedData),
					.data = std::vector<Byte>(sizeof(MaterialCombinedData))
				};

				memcpy(patch_.data.data(), &materialData, sizeof(MaterialCombinedData));

				auto& materialDataBuffer = VulkanUploadManager::GetAmazingBuffer(m_MaterialDataBufferHandle);
				materialDataBuffer.SubmitUpdate(std::move(patch_));
			}

			struct Material {
				MaterialCombinedData data;
				bool valid{ false };
				const char* name{ "Unnamed material" };
			};

			struct ShaderEffect {
				ShaderEffectData data;
				PipelineState pipelineState;
				ShaderObjectHandle shaderObjectHandle;
				bool valid{ false };
				const char* name{ "Unnamed shader effect" };
			};

			std::vector<Material> m_MaterialPool;
			std::vector<MaterialHandle> m_MaterialHoles;
			std::vector<ShaderEffect> m_ShaderEffectPool;
			std::vector<ShaderEffectHandle> m_ShaderEffectHoles;

			std::array<std::vector<MaterialHandle>, FRAMES_IN_FLIGHT> m_MaterialDestructionQueue;
			std::array<std::vector<ShaderEffectHandle>, FRAMES_IN_FLIGHT> m_ShaderEffectDestructionQueue;

			MaterialHandle m_NextMaterialHandle{ 1 };
			ShaderEffectHandle m_NextShaderEffectHandle{ 1 };

			static constexpr uint32_t MAX_MATERIALS{ 2048 };
			static constexpr uint32_t MAX_SHADER_EFFECTS{ 256 };

			AmazingBufferHandle m_MaterialDataBufferHandle;
			AmazingBufferHandle m_ShaderEffectDataBufferHandle;

			ShaderEffectSwappedCallbackFn m_ShaderEffectSwappedCallback;

			static std::unique_ptr<VulkanMaterialSystem> s_Instance;
		};
	}
}
