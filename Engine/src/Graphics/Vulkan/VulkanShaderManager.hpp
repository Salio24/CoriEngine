#pragma once
#include "VulkanEngine.hpp"
#include "VulkanLayoutManager.hpp"

namespace Cori {
	namespace Graphics {
		using ShaderObjectHandle = uint32_t;

		class ShaderObject {
		public:
			enum class Type {
				VertexFragmentPair,
				Compute
			};

			void Bind(vk::CommandBuffer& cmb) const {
				if (std::holds_alternative<vk::ShaderEXT>(m_ShaderVariant)) {
					cmb.bindShadersEXT(vk::ShaderStageFlagBits::eCompute, std::get<vk::ShaderEXT>(m_ShaderVariant));
					return;
				}

				cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, std::get<std::array<vk::ShaderEXT, 2>>(m_ShaderVariant));
			}

			[[nodiscard]] Type GetType() const {
				if (std::holds_alternative<vk::ShaderEXT>(m_ShaderVariant)) {
					return Type::Compute;
				}

				return Type::VertexFragmentPair;
			}

			[[nodiscard]] const char* GetName() const {
				return m_ShaderName;
			}

		protected:
			friend class VulkanShaderManager;
			std::variant<vk::ShaderEXT, std::array<vk::ShaderEXT, 2>> m_ShaderVariant;
			const char* m_ShaderName{ "Unnamed Shader Object" };
			bool m_Valid{ false };
		};

		class VulkanShaderManager {
		public:
			static void Init();

			static void Shutdown();

			static VulkanShaderManager& Get();

			static bool IsValid(const ShaderObjectHandle handle) {
				return handle < Get().m_Shaders.size() && Get().m_Shaders[handle].m_Valid;
			}

			static ShaderObject& GetShaderObject(const ShaderObjectHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_Shaders.size(), "Invalid ShaderObjectHandle was passed to VulkanShaderManager::GetShaderObject.");
				CORI_CORE_ASSERT(Get().m_Shaders[handle].m_Valid, "Requested ShaderObject at handle '{}' is invalid.", handle);
				return Get().m_Shaders[handle];
			}

			static void DestroyShader(const ShaderObjectHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_Shaders.size(), "Invalid ShaderObjectHandle was passed to VulkanShaderManager::GetShaderObject.");

				auto& shaderObject = Get().m_Shaders[handle];

				if (std::holds_alternative<vk::ShaderEXT>(shaderObject.m_ShaderVariant)) {
					VulkanEngine::GetLogicalDevice().destroyShaderEXT(std::get<vk::ShaderEXT>(shaderObject.m_ShaderVariant));
				} else {
					for (auto& shader : std::get<std::array<vk::ShaderEXT, 2>>(shaderObject.m_ShaderVariant)) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(shader);
					}
				}

				shaderObject.m_Valid = false;
			}

			//TODO: force the use of one source, instead of separate vertex and fragment source
			static ShaderObjectHandle CreateVertexShaderPair(const void* vertexSource, const uint64_t vertexSourceSize, const char* vertexEntryPoint, const void* fragmentSource, const uint64_t fragmentSourceSize, const char* fragmentEntryPoint, const char* shaderName = "") {
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

				ShaderObject object;
				object.m_ShaderVariant = std::array<vk::ShaderEXT, 2>{};
				auto& array = std::get<std::array<vk::ShaderEXT, 2>>(object.m_ShaderVariant);

				auto result = VulkanEngine::GetLogicalDevice().createShadersEXT(2, infos.data(), nullptr, array.data());
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Vertex Fragment pair '{}' shader creation failed. Error: {}", shaderName, vk::to_string(result));

				if (strcmp(shaderName, "") != 0) {
					object.m_ShaderName = shaderName;
				}

				VulkanEngine::SetDebugName(array[0], std::format("Vertex shader from Vertex Shader pair '{}'", object.m_ShaderName));
				VulkanEngine::SetDebugName(array[1], std::format("Fragment shader from Vertex Shader pair '{}'", object.m_ShaderName));

				if (!Get().m_Holes.empty()) {
					ShaderObjectHandle freeHandle = Get().m_Holes.back();
					Get().m_Holes.pop_back();

					Get().m_Shaders[freeHandle] = object;
					return freeHandle;
				}

				object.m_Valid = true;

				ShaderObjectHandle newHandle = Get().m_Shaders.size();
				Get().m_Shaders.emplace_back(object);
				return newHandle;
			}

			static ShaderObjectHandle CreateVertexShaderPair(const void* source, const uint64_t sourceSize, const char* vertexEntryPoint, const char* fragmentEntryPoint, const char* shaderName = "") {
				return CreateVertexShaderPair(source, sourceSize, vertexEntryPoint, source, sourceSize, fragmentEntryPoint, shaderName);
			}

			static ShaderObjectHandle CreateComputeShader(const void* source, const uint64_t sourceSize, const char* entryPoint, const char* shaderName) {
				vk::ShaderCreateInfoEXT createInfo {
					.stage = vk::ShaderStageFlagBits::eCompute,
					.codeType = vk::ShaderCodeTypeEXT::eSpirv,
					.codeSize = sourceSize,
					.pCode = source,
					.pName = entryPoint,
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
				};

				ShaderObject object;
				object.m_ShaderVariant = vk::ShaderEXT{};
				object.m_ShaderName = shaderName;
				auto& shader = std::get<vk::ShaderEXT>(object.m_ShaderVariant);

				auto result = VulkanEngine::GetLogicalDevice().createShadersEXT(1, &createInfo, nullptr, &shader);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Vertex Fragment pair '{}' shader creation failed. Error: {}", shaderName, vk::to_string(result));

				if (strcmp(shaderName, "") != 0) {
					object.m_ShaderName = shaderName;
				}

				VulkanEngine::SetDebugName(shader, std::format("Compute shader '{}'", object.m_ShaderName));

				if (!Get().m_Holes.empty()) {
					ShaderObjectHandle freeHandle = Get().m_Holes.back();
					Get().m_Holes.pop_back();

					Get().m_Shaders[freeHandle] = object;
					return freeHandle;
				}

				object.m_Valid = true;

				ShaderObjectHandle newHandle = Get().m_Shaders.size();
				Get().m_Shaders.emplace_back(object);
				return newHandle;
			}

			~VulkanShaderManager() {
				for (auto& shaderObject : m_Shaders) {
					if (std::holds_alternative<vk::ShaderEXT>(shaderObject.m_ShaderVariant)) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(std::get<vk::ShaderEXT>(shaderObject.m_ShaderVariant));
					} else {
						for (auto& shader : std::get<std::array<vk::ShaderEXT, 2>>(shaderObject.m_ShaderVariant)) {
							VulkanEngine::GetLogicalDevice().destroyShaderEXT(shader);
						}
					}
				}
			}

		private:
			VulkanShaderManager() {
				m_Shaders.reserve(32);
			}

			std::vector<ShaderObject> m_Shaders;
			std::vector<ShaderObjectHandle> m_Holes;
			static std::unique_ptr<VulkanShaderManager> s_Instance;
		};
	}
}
