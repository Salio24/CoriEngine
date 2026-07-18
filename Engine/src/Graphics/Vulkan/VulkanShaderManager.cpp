#include "VulkanShaderManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanShaderManager> VulkanShaderManager::s_Instance{nullptr};

		void VulkanShaderManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanShaderManager>(new VulkanShaderManager());
		}

		void VulkanShaderManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanShaderManager& VulkanShaderManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanShaderManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void VulkanShaderManager::RegisterAtSlot(const Core::Handle<VertFragShaderPair> handle) {
			RenderThreadCommandQueue::Push([handle]() mutable {
				if (!IsHandleValid(handle)) {
					return;
				}

				if (Get().m_PairShaders.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				Get().m_PairShaders.EmplaceAt(handle.GetIndex());
				Get().AssignPlaceholder(handle);
			});
		}

		void VulkanShaderManager::Load(const Core::Handle<ComputeShader> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			RenderThreadCommandQueue::Push([handle]() mutable {
				if (!IsHandleValid(handle)) {
					return;
				}

				if (Get().m_ComputeShaders.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				Get().m_ComputeShaders.EmplaceAt(handle.GetIndex());
			});

			auto future = Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, vectorKey]() mutable -> WorkerPayloadCompute {
				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				CORI_CORE_ASSERT(readError == glz::error_code::none, "Load({}), compute shader handle [{},{}], failed to open asset file '{}'. Error '{}'", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));

				ComputeShaderJsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				CORI_CORE_ASSERT(!parseError, "Load({}), compute shader handle [{},{}], failed to parse asset file '{}'. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));

				path.replace_filename(data.AssetData.spv);

				std::ifstream spvData(path, std::ios::ate | std::ios::binary);
				CORI_CORE_ASSERT(spvData.good(), "Load({}), compute shader handle [{},{}], failed to open spv file '{}'.", id, handle.GetIndex(), handle.GetVersion(), path.string());

				std::vector<Byte> spvBuffer(spvData.tellg());
				spvData.seekg(0, std::ios::beg);
				spvData.read(reinterpret_cast<char*>(spvBuffer.data()), static_cast<std::streamsize>(spvBuffer.size()));
				spvData.close();

				vk::ShaderCreateInfoEXT createInfo{
						.stage = vk::ShaderStageFlagBits::eCompute,
						.codeType = vk::ShaderCodeTypeEXT::eSpirv,
						.codeSize = spvBuffer.size(),
						.pCode = spvBuffer.data(),
						.pName = data.AssetData.computeEntry.c_str(),
						.pushConstantRangeCount = 1,
						.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
					};

				vk::ShaderEXT shaderObject{};
				auto vkResult = VulkanEngine::GetLogicalDevice().createShadersEXT(1, &createInfo, nullptr, &shaderObject);
				CORI_CORE_ASSERT(vkResult == vk::Result::eSuccess, "Compute shader '{}' creation failed. Error: {}.", name, vk::to_string(vkResult));

				VulkanEngine::SetDebugName(shaderObject, std::format("Compute shader '{}'", name));

				return WorkerPayloadCompute{ shaderObject };
			});

			RenderThreadCommandQueue::Push([handle, gen, future = std::move(future), vectorKey]() mutable {
				future.wait();
				if (!IsHandleValid(handle)) {
					return;
				}

				if (Get().m_ComputeShaderHandleAllocator.GetGeneration(handle) != gen) {
					return;
				}

				if (!Get().m_ComputeShaders.IsIndexOccupied(handle.GetIndex())) {
					Get().m_ComputeShaders.EmplaceAt(handle.GetIndex());
				}

				Get().DestroyShader(handle);

				auto& obj = Get().m_ComputeShaders[handle];
				auto payload = future.get();
				obj.m_ComputeShaderObject = payload.m_ComputeShader;
				payload.Release();

				SetAssetStatus(handle, AssetStatus::eLoaded);
			});
		}

		void VulkanShaderManager::Load(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			RegisterAtSlot(handle);

			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				auto FinalizeLoad = [](const Core::Handle<VertFragShaderPair> handle_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayloadPair, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([handle_, gen_, payload = std::move(payload.value()), vectorKey_]() mutable {
							if (!IsHandleValid(handle_)) {
								return;
							}

							if (Get().m_VertFragPairHandleAllocator.GetGeneration(handle_) != gen_) {
								return;
							}

							if (!Get().m_PairShaders.IsIndexOccupied(handle_.GetIndex())) {
								Get().m_PairShaders.EmplaceAt(handle_.GetIndex());
							}

							Get().DestroyShader(handle_);

							auto& obj = Get().m_PairShaders[handle_];
							obj.m_VertFragPair[0] = payload.m_VertexShader;
							obj.m_VertFragPair[1] = payload.m_FragmentShader;
							payload.Release();

							SetAssetStatus(handle_, AssetStatus::eLoaded);
						});
					}
					else {
						RenderThreadCommandQueue::Push([handle_, gen_, vectorKey_]() {
							if (!IsHandleValid(handle_)) {
								return;
							}

							if (Get().m_VertFragPairHandleAllocator.GetGeneration(handle_) != gen_) {
								return;
							}

							if (!Get().m_PairShaders.IsIndexOccupied(handle_.GetIndex())) {
								Get().m_PairShaders.EmplaceAt(handle_.GetIndex());
								Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
						});
					}
				};

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				ShaderPairJsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
					return;
				}

				path.replace_filename(data.AssetData.spv);

				std::ifstream spvData(path, std::ios::ate | std::ios::binary);

				if (!spvData.good()) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to open spv file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string());
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				std::vector<Byte> spvBuffer(spvData.tellg());
				spvData.seekg(0, std::ios::beg);
				spvData.read(reinterpret_cast<char*>(spvBuffer.data()), static_cast<std::streamsize>(spvBuffer.size()));
				spvData.close();

				std::array<vk::ShaderCreateInfoEXT, 2> infos;
				infos[0] = {
						.stage = vk::ShaderStageFlagBits::eVertex,
						.nextStage = vk::ShaderStageFlagBits::eFragment,
						.codeType = vk::ShaderCodeTypeEXT::eSpirv,
						.codeSize = spvBuffer.size(),
						.pCode = spvBuffer.data(),
						.pName = data.AssetData.vertexEntry.c_str(),
						.setLayoutCount = 1,
						.pSetLayouts = &VulkanGlobalLayoutManager::GetGlobalDescriptorSetLayout(),
						.pushConstantRangeCount = 1,
						.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
					};

				infos[1] = {
						.stage = vk::ShaderStageFlagBits::eFragment,
						.codeType = vk::ShaderCodeTypeEXT::eSpirv,
						.codeSize = spvBuffer.size(),
						.pCode = spvBuffer.data(),
						.pName = data.AssetData.fragmentEntry.c_str(),
						.setLayoutCount = 1,
						.pSetLayouts = &VulkanGlobalLayoutManager::GetGlobalDescriptorSetLayout(),
						.pushConstantRangeCount = 1,
						.pPushConstantRanges = &VulkanGlobalLayoutManager::GetGlobalPushConstantRange()
					};

				std::array<vk::ShaderEXT, 2> shaderObjects{};
				auto vkResult = VulkanEngine::GetLogicalDevice().createShadersEXT(2, infos.data(), nullptr, shaderObjects.data());
				if (vkResult != vk::Result::eSuccess) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Failed to create Vert+Frag Shader pair '{}'. Error: {}. Using placeholder.", name, vk::to_string(vkResult));
					if (shaderObjects[0] != nullptr) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObjects[0]);
					}

					if (shaderObjects[1] != nullptr) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObjects[1]);
					}

					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eCreationFailure));
					return;
				}

				VulkanEngine::SetDebugName(shaderObjects[0], std::format("Vertex shader from Shader Pair '{}'", name));
				VulkanEngine::SetDebugName(shaderObjects[1], std::format("Fragment shader from Shader Pair '{}'", name));

				FinalizeLoad(handle, gen, vectorKey, WorkerPayloadPair{ shaderObjects[0], shaderObjects[1] });
			});
		}
	}
}
