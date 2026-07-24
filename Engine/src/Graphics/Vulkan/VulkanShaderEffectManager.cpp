#include "VulkanShaderEffectManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanShaderEffectManager> VulkanShaderEffectManager::s_Instance{ nullptr };

		void VulkanShaderEffectManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderEffectManager is already initialized.");
			s_Instance = std::unique_ptr<VulkanShaderEffectManager>(new VulkanShaderEffectManager());
		}

		void VulkanShaderEffectManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanShaderEffectManager& VulkanShaderEffectManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanShaderEffectManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void VulkanShaderEffectManager::RegisterAtSlot(const Core::Handle<ShaderEffect> handle) {
			RenderThreadCommandQueue::Push([handle]() mutable {
				if (!IsHandleValid(handle)) {
					return;
				}

				if (Get().m_ShaderEffects.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				uint32_t index = handle.GetIndex();
				uint32_t size = Get().m_ShaderEffectData.Size();
				if (index >= size) {
					Get().m_ShaderEffectData.Resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
				}

				Get().m_ShaderEffects.EmplaceAt(handle.GetIndex(), Get().m_ShaderEffects[Get().m_PlaceholderEffect]);
				//Get().AssignPlaceholder(handle);
			});
		}

		void VulkanShaderEffectManager::Load(const Core::Handle<ShaderEffect> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			RegisterAtSlot(handle);
			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				auto FinalizeLoad = [](const Core::Handle<ShaderEffect> handle_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayload, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([handle_, gen_, payload = std::move(payload.value()), vectorKey_]() mutable {
							if (!IsHandleValid(handle_)) {
								return;
							}

							if (Get().m_HandleAllocator.GetGeneration(handle_) != gen_) {
								return;
							}

							if (!Get().m_ShaderEffects.IsIndexOccupied(handle_.GetIndex())) {
								uint32_t index = handle_.GetIndex();
								uint32_t size = Get().m_ShaderEffectData.Size();
								if (index >= size) {
									Get().m_ShaderEffectData.Resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
								}

								VulkanShaderManager::RegisterAtSlot(payload.effect.shaders.GetHandle());
								Get().m_ShaderEffects.EmplaceAt(handle_.GetIndex(), std::move(payload.effect));
							} else {
								VulkanShaderManager::RegisterAtSlot(payload.effect.shaders.GetHandle());
								Get().m_ShaderEffects[handle_] = std::move(payload.effect);
							}

							Get().m_ShaderEffectData[handle_.GetIndex()] = payload.data;

							SetAssetStatus(handle_, AssetStatus::eLoaded);
						});
					}
					else {
						RenderThreadCommandQueue::Push([handle_, gen_, vectorKey_]() {
							if (!IsHandleValid(handle_)) {
								return;
							}

							if (Get().m_HandleAllocator.GetGeneration(handle_) != gen_) {
								return;
							}

							if (!Get().m_ShaderEffects.IsIndexOccupied(handle_.GetIndex())) {
								uint32_t index = handle_.GetIndex();
								uint32_t size = Get().m_ShaderEffectData.Size();
								if (index >= size) {
									Get().m_ShaderEffectData.Resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
								}

								Get().m_ShaderEffects.EmplaceAt(handle_.GetIndex(), Get().m_ShaderEffects[Get().m_PlaceholderEffect]);
								//Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
						});
					}
				};

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Load({}), handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderEffectManager }, "Load({}), handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
					return;
				}

				ShaderEffectData customData{
					.custom1 = { data.AssetData.customData.custom1.value_or(0.0f), data.AssetData.customData.custom2.value_or(0.0f), data.AssetData.customData.custom3.value_or(0.0f), data.AssetData.customData.custom4.value_or(0.0f) },
					.custom2 = { data.AssetData.customData.custom5.value_or(0.0f), data.AssetData.customData.custom6.value_or(0.0f), data.AssetData.customData.custom7.value_or(0.0f), data.AssetData.customData.custom8.value_or(0.0f) }
				};

				PipelineState state{
					.cullMode = data.AssetData.pipelineState.cullMode,
					.frontFace = data.AssetData.pipelineState.frontFace,
					.depthCompareOp = data.AssetData.pipelineState.depthCompareOp,
					.depthTestEnable = data.AssetData.pipelineState.depthTestEnable,
					.depthBoundsTestEnable = data.AssetData.pipelineState.depthBoundsTestEnable,
					.depthBiasEnable = data.AssetData.pipelineState.depthBiasEnable,
					.stencilTestEnable = data.AssetData.pipelineState.stencilTestEnable,
					.logicOpEnable = data.AssetData.pipelineState.logicOpEnable
				};

				FinalizeLoad(handle, gen, vectorKey, WorkerPayload{
					.effect = ShaderEffect{ .shaders = std::move(data.AssetData.shaderPair), .pipelineState = state },
					.data = customData
				});
			});
		}
	}
}
