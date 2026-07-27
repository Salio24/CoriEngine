#include "VulkanShaderManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanShaderManager> VulkanShaderManager::s_Instance{nullptr};

		VulkanShaderManager::VulkanShaderManager() {
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

			m_PairShaders.EmplaceAt(m_PlaceholderShaderPair.GetIndex());
			CreateShaderPair(m_PlaceholderShaderPair, buffer.data(), buffer.size(), "vertMain", buffer.data(), buffer.size(), "fragMain", "Placeholder Vert+Frag Shader Pair");

			CORI_CORE_ASSERT(m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[0] && m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[1], "Placeholder Vert+Frag shader pair creation failed.");
			m_PlaceholderVertexShader = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[0];
			m_PlaceholderFragmentShader = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair[1];
		}

		VulkanShaderManager::~VulkanShaderManager() {
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

		void VulkanShaderManager::BindAsset(const Core::Handle<ComputeShader> handle, const Core::AssetID id, const uint32_t vectorKey) {
			return Get().m_ComputeShaderHandleAllocator.BindAsset(handle, id, vectorKey);
		}

		void VulkanShaderManager::BindAsset(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id, const uint32_t vectorKey) {
			return Get().m_VertFragPairHandleAllocator.BindAsset(handle, id, vectorKey);
		}

		uint32_t VulkanShaderManager::BumpGeneration(const Core::Handle<ComputeShader> handle) {
			return Get().m_ComputeShaderHandleAllocator.BumpGeneration(handle);
		}

		uint32_t VulkanShaderManager::BumpGeneration(const Core::Handle<VertFragShaderPair> handle) {
			return Get().m_VertFragPairHandleAllocator.BumpGeneration(handle);
		}

		bool VulkanShaderManager::IsHandleValid(const Core::ConstHandle<ComputeShader> handle) {
			return Get().m_ComputeShaderHandleAllocator.IsHandleValid(handle);
		}

		bool VulkanShaderManager::IsHandleValid(const Core::ConstHandle<VertFragShaderPair> handle) {
			return Get().m_VertFragPairHandleAllocator.IsHandleValid(handle);
		}

		Core::AssetID VulkanShaderManager::GetAssetID(const Core::Handle<ComputeShader> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Compute shader handle passed to GetAssetID is invalid.");
			return Get().m_ComputeShaderHandleAllocator.GetBoundAssetID(handle);
		}

		Core::AssetID VulkanShaderManager::GetAssetID(const Core::Handle<VertFragShaderPair> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Shader pair handle passed to GetAssetID is invalid.");
			return Get().m_VertFragPairHandleAllocator.GetBoundAssetID(handle);
		}

		bool VulkanShaderManager::TryAddRef(const Core::Handle<ComputeShader> handle) {
			return Get().m_ComputeShaderHandleAllocator.TryAddRef(handle);
		}

		bool VulkanShaderManager::TryAddRef(const Core::Handle<VertFragShaderPair> handle) {
			return Get().m_VertFragPairHandleAllocator.TryAddRef(handle);
		}

		void VulkanShaderManager::AddRef(const Core::Handle<ComputeShader> handle) {
			Get().m_ComputeShaderHandleAllocator.AddRef(handle);
		}

		void VulkanShaderManager::AddRef(const Core::Handle<VertFragShaderPair> handle) {
			Get().m_VertFragPairHandleAllocator.AddRef(handle);
		}

		void VulkanShaderManager::RemoveRef(const Core::Handle<ComputeShader> handle) {
			Get().m_ComputeShaderHandleAllocator.RemoveRef(handle);
		}

		void VulkanShaderManager::RemoveRef(const Core::Handle<VertFragShaderPair> handle) {
			Get().m_VertFragPairHandleAllocator.RemoveRef(handle);
		}

		void VulkanShaderManager::SetAssetStatus(const Core::Handle<ComputeShader> handle, const AssetStatus newStatus) {
			Get().m_ComputeShaderHandleAllocator.SetAssetStatus(handle, newStatus);
		}

		void VulkanShaderManager::SetAssetStatus(const Core::Handle<VertFragShaderPair> handle, const AssetStatus newStatus) {
			Get().m_VertFragPairHandleAllocator.SetAssetStatus(handle, newStatus);
		}

		AssetStatus VulkanShaderManager::GetAssetStatus(const Core::Handle<ComputeShader> handle) {
			return Get().m_ComputeShaderHandleAllocator.GetAssetStatus(handle);
		}

		AssetStatus VulkanShaderManager::GetAssetStatus(const Core::Handle<VertFragShaderPair> handle) {
			return Get().m_VertFragPairHandleAllocator.GetAssetStatus(handle);
		}

		void VulkanShaderManager::RegisterAtSlot([[maybe_unused]] const Core::Handle<ComputeShader> handle) {
			//empty cuz compute shaders are guaranteed to be loaded next frame cuz there is no sticky placeholder for them. They are kind of an exception.
		}

		void VulkanShaderManager::RegisterAtSlot(const Core::Handle<VertFragShaderPair> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Register);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

			RenderThreadCommandQueue::Push([handle]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Shader pair RegisterAtSlot", Cori::ProfileColors::Register);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

				if (!IsHandleValid(handle)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid");
					return;
				}

				if (Get().m_PairShaders.IsIndexOccupied(handle.GetIndex())) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Index already occupied");
					return;
				}

				Get().m_PairShaders.EmplaceAt(handle.GetIndex());
				Get().AssignPlaceholder(handle);
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Emplaced and PLACEHOLDER shader pair assigned");
			});
		}

		void VulkanShaderManager::Load(const Core::Handle<ComputeShader> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] LOAD requested (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

			auto future = Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable -> WorkerPayloadCompute {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Worker Task: Compute shader parse/spv read/creation", Cori::ProfileColors::Worker);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u path=%s", handle.GetIndex(), handle.GetVersion(), gen, path.string().c_str());
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] worker begin (parse/spv read/create), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

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

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "SPIR-V: %s (%llu bytes), compute entry '%s'", data.AssetData.spv.c_str(), static_cast<unsigned long long>(spvBuffer.size()), data.AssetData.computeEntry.c_str());

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
				auto vkResult = [&] {
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "createShadersEXT (compute)", Cori::ProfileColors::Upload);
					return VulkanEngine::GetLogicalDevice().createShadersEXT(1, &createInfo, nullptr, &shaderObject);
				}();
				CORI_CORE_ASSERT(vkResult == vk::Result::eSuccess, "Compute shader '{}' creation failed. Error: {}.", name, vk::to_string(vkResult));

				VulkanEngine::SetDebugName(shaderObject, std::format("Compute shader '{}'", name));

				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Compute shader object created, handed to the render thread through the future");
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] compute shader object created (%llu spv bytes), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion(), static_cast<unsigned long long>(spvBuffer.size()), gen, static_cast<unsigned long long>(id));

				return WorkerPayloadCompute{ shaderObject };
			});

			RenderThreadCommandQueue::Push([handle, id, gen, future = std::move(future), vectorKey]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Compute shader FinalizeLoad", Cori::ProfileColors::Finalize);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

				if (!IsHandleValid(handle)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, compute shader object discarded (destroyed by the payload dtor)");
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: handle invalid (compute shader object discarded), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
					return;
				}

				const uint32_t currentGen = Get().m_ComputeShaderHandleAllocator.GetGeneration(handle);
				if (currentGen != gen) {
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), compute shader object discarded (destroyed by the payload dtor)", gen, currentGen);
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion(), gen, currentGen, static_cast<unsigned long long>(id));
					return;
				}

				if (!Get().m_ComputeShaders.IsIndexOccupied(handle.GetIndex())) {
					Get().m_ComputeShaders.EmplaceAt(handle.GetIndex());
				}

				Get().DestroyShader(handle);

				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Wait on compute shader worker future", Cori::ProfileColors::Worker);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] blocking the render thread until the worker is done", handle.GetIndex(), handle.GetVersion());
					future.wait();
				}

				auto& obj = Get().m_ComputeShaders[handle];
				auto payload = future.get();
				obj.m_ComputeShaderObject = payload.m_ComputeShader;
				payload.Release();

				SetAssetStatus(handle, AssetStatus::eLoaded);
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: LOADED, real compute shader now bindable");
				CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded, "%s Handle=[%u, %u] LOADED, real compute shader now bindable (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
			});
		}

		void VulkanShaderManager::Load(const Core::Handle<VertFragShaderPair> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] LOAD requested (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

			RegisterAtSlot(handle);

			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Worker Task: Shader pair parse/spv read/creation", Cori::ProfileColors::Worker);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u path=%s", handle.GetIndex(), handle.GetVersion(), gen, path.string().c_str());
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] worker begin (parse/spv read/create), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

				auto FinalizeLoad = [](const Core::Handle<VertFragShaderPair> handle_, const Core::AssetID id_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayloadPair, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([id_, handle_, gen_, payload = std::move(payload.value()), vectorKey_]() mutable {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Shader pair FinalizeLoad", Cori::ProfileColors::Finalize);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));

							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, dropped shader pair (objects destroyed by the payload dtor)");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: handle invalid (shader objects discarded), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_VertFragPairHandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), dropped shader pair (objects destroyed by the payload dtor)", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
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
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: LOADED, real vert+frag pair now bindable");
							CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded, "%s Handle=[%u, %u] LOADED, real vert+frag pair now bindable (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
						});
					}
					else {
						RenderThreadCommandQueue::Push([id_, handle_, gen_, vectorKey_]() {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Shader pair FinalizeLoad (fail)", Cori::ProfileColors::Destroy);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));

							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, skipped");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: handle invalid, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_VertFragPairHandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), skipped", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
								return;
							}

							if (!Get().m_PairShaders.IsIndexOccupied(handle_.GetIndex())) {
								Get().m_PairShaders.EmplaceAt(handle_.GetIndex());
								Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: status=LoadFailed, PLACEHOLDER shader pair shown");
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED, status=LoadFailed, PLACEHOLDER shader pair shown, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
						});
					}
				};

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eFailedToOpenFile).data());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: cannot open asset file '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
					FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				ShaderPairJsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eParseFailure).data());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: asset json '%s' parse error, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
					FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
					return;
				}

				path.replace_filename(data.AssetData.spv);

				std::ifstream spvData(path, std::ios::ate | std::ios::binary);

				if (!spvData.good()) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Load({}), shader pair handle [{},{}], failed to open spv file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string());
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eFailedToOpenFile).data());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: cannot open spv file '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
					FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				std::vector<Byte> spvBuffer(spvData.tellg());
				spvData.seekg(0, std::ios::beg);
				spvData.read(reinterpret_cast<char*>(spvBuffer.data()), static_cast<std::streamsize>(spvBuffer.size()));
				spvData.close();

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "SPIR-V: %s (%llu bytes), vertex entry '%s', fragment entry '%s'", data.AssetData.spv.c_str(), static_cast<unsigned long long>(spvBuffer.size()), data.AssetData.vertexEntry.c_str(), data.AssetData.fragmentEntry.c_str());

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
				auto vkResult = [&] {
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "createShadersEXT (vert+frag)", Cori::ProfileColors::Upload);
					return VulkanEngine::GetLogicalDevice().createShadersEXT(2, infos.data(), nullptr, shaderObjects.data());
				}();

				if (vkResult != vk::Result::eSuccess) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::ShaderManager }, "Failed to create Vert+Frag Shader pair '{}'. Error: {}. Using placeholder.", name, vk::to_string(vkResult));
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: createShadersEXT failed (%s), enqueuing FinalizeLoad(fail)", vk::to_string(vkResult).c_str());
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: createShadersEXT returned %s for '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), vk::to_string(vkResult).c_str(), path.c_str(), gen, static_cast<unsigned long long>(id));

					if (shaderObjects[0] != nullptr) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObjects[0]);
					}

					if (shaderObjects[1] != nullptr) {
						VulkanEngine::GetLogicalDevice().destroyShaderEXT(shaderObjects[1]);
					}

					FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eCreationFailure));
					return;
				}

				VulkanEngine::SetDebugName(shaderObjects[0], std::format("Vertex shader from Shader Pair '{}'", name));
				VulkanEngine::SetDebugName(shaderObjects[1], std::format("Fragment shader from Shader Pair '{}'", name));

				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Vert+frag shader objects created, handed to FinalizeLoad");
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] vert+frag shader objects created (%llu spv bytes), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), static_cast<unsigned long long>(spvBuffer.size()), gen, static_cast<unsigned long long>(id));

				FinalizeLoad(handle, id, gen, vectorKey, WorkerPayloadPair{ shaderObjects[0], shaderObjects[1] });
			});
		}

		void VulkanShaderManager::Unload(const Core::Handle<ComputeShader> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UNLOAD (destroying compute shader, freeing handle)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion());
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");

			Core::AssetID id = Get().m_ComputeShaderHandleAllocator.GetBoundAssetID(handle);
			{
				auto& mutex = Core::AssetManager2::GetMutex();
				std::lock_guard lk(mutex);
				CORI_PROFILER_LOCK_MARK(mutex);
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				if (record.rawHandleIndex == handle.GetIndex() && record.rawHandleVersion == handle.GetVersion()) {
					record.rawHandleIndex = UINT32_MAX;
					record.rawHandleVersion = 0;
				}
			}

			Get().m_ComputeShaderHandleAllocator.Free(handle);

			if (!Get().m_ComputeShaders.IsIndexOccupied(handle.GetIndex())) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, no compute shader slot was occupied");
				return;
			}

			Get().DestroyShader(handle);
			Get().m_ComputeShaders.RemoveAt(handle.GetIndex());
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, compute shader destroyed and slot released");
		}

		void VulkanShaderManager::Unload(const Core::Handle<VertFragShaderPair> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UNLOAD (destroying shader pair, freeing handle)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion());
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
			CORI_CORE_ASSERT(handle != Get().m_PlaceholderShaderPair, "Placeholder shader pair handle was passed, can't unload it.")

			Core::AssetID id = Get().m_VertFragPairHandleAllocator.GetBoundAssetID(handle);
			{
				auto& mutex = Core::AssetManager2::GetMutex();
				std::lock_guard lk(mutex);
				CORI_PROFILER_LOCK_MARK(mutex);
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				if (record.rawHandleIndex == handle.GetIndex() && record.rawHandleVersion == handle.GetVersion()) {
					record.rawHandleIndex = UINT32_MAX;
					record.rawHandleVersion = 0;
				}
			}

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Notifying %llu deletion listener(s) (id=%llu)", static_cast<unsigned long long>(Get().m_Listeners.size()), static_cast<unsigned long long>(id));

			for (auto& [ptr, func] : Get().m_Listeners) {
				func(ptr, handle);
			}

			Get().m_VertFragPairHandleAllocator.Free(handle);

			if (!Get().m_PairShaders.IsIndexOccupied(handle.GetIndex())) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, no shader pair slot was occupied");
				return;
			}

			Get().DestroyShader(handle);
			Get().m_PairShaders.RemoveAt(handle.GetIndex());
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, shader pair destroyed and slot released");
		}

		void VulkanShaderManager::QueueUnload(const Core::Handle<ComputeShader> handle) {
			RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
		}

		void VulkanShaderManager::QueueUnload(const Core::Handle<VertFragShaderPair> handle) {
			RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
		}

		void VulkanShaderManager::Bind(const Core::ConstHandle<ComputeShader> handle, vk::CommandBuffer cmb) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Bind is invalid.");

			cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eCompute }, Get().m_ComputeShaders[handle].m_ComputeShaderObject);
		}

		void VulkanShaderManager::Bind(const Core::ConstHandle<VertFragShaderPair> handle, vk::CommandBuffer cmb) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Bind is invalid.");

			cmb.bindShadersEXT({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, Get().m_PairShaders[handle].m_VertFragPair);
		}

		void VulkanShaderManager::AddOnVertFragShaderPairDeletedListener(void* instance, OnVertFragShaderPairDeletedFn func) {
			Get().m_Listeners.emplace_back(instance, std::move(func));
		}

		void VulkanShaderManager::RemoveOnVertFragShaderPairDeletedListener(const void* instance) {
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

		void VulkanShaderManager::ClearOnVertFragShaderPairDeletedListener() {
			Get().m_Listeners.clear();
		}

		void VulkanShaderManager::AssignPlaceholder(const Core::Handle<VertFragShaderPair> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
			CORI_CORE_ASSERT(IsHandleValid(handle), "Shader pair handle passed to AssignPlaceholder is invalid.");

			auto& object = m_PairShaders[handle];
			object.m_VertFragPair = m_PairShaders[m_PlaceholderShaderPair].m_VertFragPair;
			object.placeholderAssigned = true;

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> PLACEHOLDER shader pair (borrowed from handle [%u, %u], objects not owned)", handle.GetIndex(), handle.GetVersion(), m_PlaceholderShaderPair.GetIndex(), m_PlaceholderShaderPair.GetVersion());
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned PLACEHOLDER shader pair (borrowed from handle [%u, %u])", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), m_PlaceholderShaderPair.GetIndex(), m_PlaceholderShaderPair.GetVersion());
		}

		void VulkanShaderManager::CreateShaderPair(const Core::Handle<VertFragShaderPair> handle, const void* vertexSource, const uint64_t vertexSourceSize, const char* vertexEntryPoint, const void* fragmentSource, const uint64_t fragmentSourceSize, const char* fragmentEntryPoint, const char* shaderName) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] '%s', vertex entry '%s' (%llu bytes), fragment entry '%s' (%llu bytes)", handle.GetIndex(), handle.GetVersion(), shaderName, vertexEntryPoint, static_cast<unsigned long long>(vertexSourceSize), fragmentEntryPoint, static_cast<unsigned long long>(fragmentSourceSize));

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

			auto result = [&] {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "createShadersEXT (vert+frag)", Cori::ProfileColors::Upload);
				return VulkanEngine::GetLogicalDevice().createShadersEXT(2, infos.data(), nullptr, shaderObjects.data());
			}();
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create placeholder shader pair.")

			object.m_VertFragPair = shaderObjects;

			VulkanEngine::SetDebugName(object.m_VertFragPair[0], std::format("Vertex shader from Vertex Shader pair '{}'", shaderName));
			VulkanEngine::SetDebugName(object.m_VertFragPair[1], std::format("Fragment shader from Vertex Shader pair '{}'", shaderName));

			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Shader pair created directly into the slot (no streaming, blocking creation)");
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] CREATED in place '%s' (vertex entry '%s', fragment entry '%s')", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), shaderName, vertexEntryPoint, fragmentEntryPoint);
		}

		void VulkanShaderManager::DestroyShader(const Core::Handle<ComputeShader> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			auto& object = m_ComputeShaders[handle];

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] (placeholderAssigned=%d, hasObject=%d)", handle.GetIndex(), handle.GetVersion(), static_cast<int>(object.placeholderAssigned), static_cast<int>(object.m_ComputeShaderObject != nullptr));
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] DestroyShader (placeholderAssigned=%d, hasObject=%d)", CORI_CLEAN_TYPE_NAME(ComputeShader), handle.GetIndex(), handle.GetVersion(), static_cast<int>(object.placeholderAssigned), static_cast<int>(object.m_ComputeShaderObject != nullptr));

			if (!object.placeholderAssigned) {
				if (m_ComputeShaders[handle].m_ComputeShaderObject != nullptr) {
					DeletionQueue::PushShaderObject(m_ComputeShaders[handle].m_ComputeShaderObject);
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Queued the compute shader object for deletion");
				}
			}

			object.placeholderAssigned = false;
		}

		void VulkanShaderManager::DestroyShader(const Core::Handle<VertFragShaderPair> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			if (m_PlaceholderShaderPair == handle) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Skipped: placeholder shader pair is never destroyed");
				return;
			}

			auto& pair = m_PairShaders[handle];

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] (placeholderAssigned=%d, hasVertex=%d, hasFragment=%d)", handle.GetIndex(), handle.GetVersion(), static_cast<int>(pair.placeholderAssigned), static_cast<int>(pair.m_VertFragPair[0] != nullptr), static_cast<int>(pair.m_VertFragPair[1] != nullptr));
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] DestroyShader (placeholderAssigned=%d, hasVertex=%d, hasFragment=%d)", CORI_CLEAN_TYPE_NAME(VertFragShaderPair), handle.GetIndex(), handle.GetVersion(), static_cast<int>(pair.placeholderAssigned), static_cast<int>(pair.m_VertFragPair[0] != nullptr), static_cast<int>(pair.m_VertFragPair[1] != nullptr));

			if (!pair.placeholderAssigned) {
				if (pair.m_VertFragPair[0] != nullptr) {
					DeletionQueue::PushShaderObject(pair.m_VertFragPair[0]);
				}

				if (pair.m_VertFragPair[1] != nullptr) {
					DeletionQueue::PushShaderObject(pair.m_VertFragPair[1]);
				}

				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Queued the owned vert+frag shader objects for deletion");
			} else {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Nothing deleted: the pair only borrowed the placeholder objects");
			}

			pair.placeholderAssigned = false;
		}
	}
}
