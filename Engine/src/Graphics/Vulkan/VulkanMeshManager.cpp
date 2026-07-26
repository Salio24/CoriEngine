#include "VulkanMeshManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanMeshManager> VulkanMeshManager::s_Instance{ nullptr };

		void VulkanMeshManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderManager is already initialized.")
			s_Instance = std::unique_ptr<VulkanMeshManager>(new VulkanMeshManager());
		}

		void VulkanMeshManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanMeshManager& VulkanMeshManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanMeshManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void VulkanMeshManager::RegisterAtSlot(const Core::Handle<Mesh> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Register);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

			RenderThreadCommandQueue::Push([handle]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Mesh RegisterAtSlot", Cori::ProfileColors::Register);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());

				if (!IsHandleValid(handle)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid");
					return;
				}

				if (Get().m_Meshes.IsIndexOccupied(handle.GetIndex())) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Index already occupied");
					return;
				}

				const auto [prevMetaSize, newMetaSize] = Get().EnsureMetadataSlot(handle.GetIndex());
				if (prevMetaSize != newMetaSize) {
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Grew mesh metadata %u -> %u entries (index %u)", prevMetaSize, newMetaSize, handle.GetIndex());
				}

				Get().m_Meshes.EmplaceAt(handle.GetIndex());
				Get().AssignEmptyMesh(handle);
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Emplaced and empty placeholder assigned");
			});
		}

		void VulkanMeshManager::Load(const Core::Handle<Mesh> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Load, "%s Handle=[%u, %u] LOAD requested (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

			RegisterAtSlot(handle);
			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Worker Task: Mesh parse/decode/vertex allocation", Cori::ProfileColors::Worker);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u path=%s", handle.GetIndex(), handle.GetVersion(), gen, path.string().c_str());
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Worker, "%s Handle=[%u, %u] worker begin (parse/decode/vertex alloc), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), gen, static_cast<unsigned long long>(id));

				auto FinalizeLoad = [](const Core::Handle<Mesh> handle_, const Core::AssetID id_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayload, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([id_, handle_, gen_, payload = std::move(payload.value())]() mutable {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Mesh FinalizeLoad", Cori::ProfileColors::Finalize);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));

							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, dropped parsed mesh (vertex allocation released by payload dtor)");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: handle invalid (parsed mesh discarded), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_HandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), dropped parsed mesh (vertex allocation released by payload dtor)", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize dropped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
								return;
							}

							if (!Get().m_Meshes.IsIndexOccupied(handle_.GetIndex())) {
								const auto [prevMetaSize, newMetaSize] = Get().EnsureMetadataSlot(handle_.GetIndex());
								if (prevMetaSize != newMetaSize) {
									CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Grew mesh metadata %u -> %u entries (index %u)", prevMetaSize, newMetaSize, handle_.GetIndex());
								}

								Get().m_Meshes.EmplaceAt(handle_.GetIndex());
							}

							Get().DestroyMesh(handle_);
							auto& meta = Get().m_MeshMetadata[handle_.GetIndex()];
							meta.placeholderAssigned = false;

							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handing worker vertex allocation (storage=%p @ byte offset %llu) to LoadToMesh", static_cast<void*>(payload.m_CompleteVertexAlloc.storage), static_cast<unsigned long long>(payload.m_CompleteVertexAlloc.offset));
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Finalize, "%s Handle=[%u, %u] finalize: uploading parsed mesh (storage=%p, gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle_.GetIndex(), handle_.GetVersion(), static_cast<void*>(payload.m_CompleteVertexAlloc.storage), gen_, static_cast<unsigned long long>(id_));

							Get().LoadToMesh(handle_, std::move(std::get<std::vector<StaticVertex>>(payload.m_VertexData)), std::move(payload.m_IndexData), gen_, payload.m_CompleteVertexAlloc);

							payload.Release();
						});
					}
					else {
						RenderThreadCommandQueue::Push([id_, handle_, gen_, vectorKey_]() {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "RTCQ Task: Mesh FinalizeLoad (fail)", Cori::ProfileColors::Destroy);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] gen=%u id=%llu", handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));

							if (!IsHandleValid(handle_)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, skipped");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: handle invalid, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
								return;
							}

							const uint32_t currentGen = Get().m_HandleAllocator.GetGeneration(handle_);
							if (currentGen != gen_) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (expected gen=%u, current gen=%u), skipped", gen_, currentGen);
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] finalize(fail) skipped: STALE (expected gen=%u, current gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle_.GetIndex(), handle_.GetVersion(), gen_, currentGen, static_cast<unsigned long long>(id_));
								return;
							}

							if (!Get().m_Meshes.IsIndexOccupied(handle_.GetIndex())) {
								const auto [prevMetaSize, newMetaSize] = Get().EnsureMetadataSlot(handle_.GetIndex());
								if (prevMetaSize != newMetaSize) {
									CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Grew mesh metadata %u -> %u entries (index %u)", prevMetaSize, newMetaSize, handle_.GetIndex());
								}

								Get().m_Meshes.EmplaceAt(handle_.GetIndex());
								Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: status=LoadFailed, PLACEHOLDER mesh shown");
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED, status=LoadFailed, PLACEHOLDER mesh shown, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle_.GetIndex(), handle_.GetVersion(), gen_, static_cast<unsigned long long>(id_));
						});
					}
				};

				JsonAssetDataCombined data;
				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Mesh json parse", Cori::ProfileColors::Decode);
					std::string buffer;
					auto readError = glz::file_to_buffer(buffer, path.c_str());
					if (readError != glz::error_code::none) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Load({}), mesh handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eFailedToOpenFile).data());
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: cannot open asset file '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
						FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
						return;
					}

					auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
					if (parseError) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Load({}), mesh handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eParseFailure).data());
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: asset json '%s' parse error, (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), path.c_str(), gen, static_cast<unsigned long long>(id));
						FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
						return;
					}
				}

				std::filesystem::path objPath = path.replace_filename(data.AssetData.obj);
				std::vector<StaticVertex> vertexData;
				std::vector<uint32_t> indexData;
				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Obj load", Cori::ProfileColors::Decode);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Obj file: %s", data.AssetData.obj.c_str());

					if (!LoadObjToEngine(objPath.string().c_str(), vertexData, indexData)) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Load({}), mesh handle [{},{}], failed to read obj file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), objPath.string());
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Decode/load failed (error=%s), enqueuing FinalizeLoad(fail)", to_string(ErrorCode::eParseFailure).data());
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LOAD FAILED: cannot read obj file '%s', (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), objPath.c_str(), gen, static_cast<unsigned long long>(id));
						FinalizeLoad(handle, id, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
						return;
					}

					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Parsed %llu vertices (%llu bytes), %llu indices (%llu bytes)", static_cast<unsigned long long>(vertexData.size()), static_cast<unsigned long long>(vertexData.size() * sizeof(StaticVertex)), static_cast<unsigned long long>(indexData.size()), static_cast<unsigned long long>(indexData.size() * sizeof(uint32_t)));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Decode, "%s Handle=[%u, %u] parsed obj: %llu vertices (%llu bytes), %llu indices (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), static_cast<unsigned long long>(vertexData.size()), static_cast<unsigned long long>(vertexData.size() * sizeof(StaticVertex)), static_cast<unsigned long long>(indexData.size()), gen, static_cast<unsigned long long>(id));
				}

				CompleteVertexAllocation ca;

				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "Vertex storage suballocation and FinalizeLoad submit", Cori::ProfileColors::Upload);

					//this will need to change to support multiple vertex layouts
					vma::VirtualAllocationCreateInfo verticesAllocInfo {
						.size = vertexData.size() * sizeof(StaticVertex),
						.alignment = alignof(StaticVertex),
						.flags = s_VertexAllocFlags
					};

					[[maybe_unused]] uint32_t storagesProbed = 0;
					for (auto& storage : Get().m_VertexStorages) {
						storagesProbed++;
						if (!storage.DiscoverBoth()) {
							continue;
						}

						{
							std::lock_guard lk(*storage.m_Mutex);

							auto [result_, alloc] = storage.m_Block.virtualAllocate(verticesAllocInfo, ca.offset);
							if (result_ == vk::Result::eSuccess) {
								ca.allocation = alloc;
								ca.storage = &storage;
								break;
							}
						}

						storage.RemoveStrongRef();
						storage.RemoveWeakRef();
					}

					if (!ca.storage) {
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "No room in %u existing storage(s), allocating a new one", storagesProbed);
						VertexStorage* vertexStorage = Get().AllocateNewVertexStorage();
						auto discoverResult = vertexStorage->DiscoverBoth();
						CORI_CORE_ASSERT(discoverResult, "Discovery failed for newly created vertex block for some reason.");

						std::lock_guard lk(*vertexStorage->m_Mutex);
						auto [result2, alloc] = vertexStorage->m_Block.virtualAllocate(verticesAllocInfo, ca.offset);
						CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new vertices in a newly created vertex storage, error: {}", vk::to_string(result2));
						ca.allocation = alloc;
						ca.storage = vertexStorage;
					} else {
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Suballocated from existing storage after probing %u storage(s)", storagesProbed);
					}

					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Parsed, vertex allocation taken (storage=%p @ byte offset %llu), handed to FinalizeLoad", static_cast<void*>(ca.storage), static_cast<unsigned long long>(ca.offset));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] worker took vertex allocation (storage=%p @ byte offset %llu), (gen=%u) (id=%llu)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), static_cast<void*>(ca.storage), static_cast<unsigned long long>(ca.offset), gen, static_cast<unsigned long long>(id));

					FinalizeLoad(handle, id, gen, vectorKey, WorkerPayload(std::move(vertexData), std::move(indexData), ca));
				}
			});
		}
	}
}