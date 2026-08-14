#include "VulkanMeshManager.hpp"
#include "Core/Application.hpp"

static uint64_t counter = 0;

namespace Cori {
	namespace Graphics {
		std::unique_ptr<VulkanMeshManager> VulkanMeshManager::s_Instance{ nullptr };

		VulkanMeshManager::VulkanMeshManager() {
			s_Instance = std::unique_ptr<VulkanMeshManager>(this);
			auto& sharingSettings = VulkanEngine::GetBufferSharingSettings(BUFFER_USAGE);

			vk::BufferCreateInfo vkIndexBufferInfo {
				.size = INDEX_STORAGE_SIZE,
				.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
				.sharingMode = sharingSettings.first,
				.queueFamilyIndexCount = static_cast<uint32_t>(sharingSettings.second.size()),
				.pQueueFamilyIndices = sharingSettings.second.data()
			};

			vma::AllocationCreateInfo allocCreateInfo {
				.flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
				.usage = vma::MemoryUsage::eAuto
			};

			VulkanBuffer::CreateInfo indexBufferInfo {
				.bufferCreateInfo = &vkIndexBufferInfo,
				.allocationCreateInfo = &allocCreateInfo,
				.name = "MeshManager index buffer"
			};

			m_IndexBuffer = VulkanBuffer::Create(indexBufferInfo);

			vma::VirtualBlockCreateInfo indexBufferBlockInfo {
				.size = INDEX_STORAGE_SIZE,
			};

			auto [result_, indexBufferBlock] = vma::createVirtualBlock(indexBufferBlockInfo);

			CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create vma index buffer virtual block. Error: {}", vk::to_string(result_));

			m_IndexBufferBlock = indexBufferBlock;

			m_Meshes.Reserve(512);
			m_MeshMetadata.resize(512);

			m_PlaceholderMesh = m_HandleAllocator.Allocate();
			m_HandleAllocator.AddRef(m_PlaceholderMesh);

			std::vector<StaticVertex> placeholderVertexData{
				// +Y
				{{1, 1, -1}, 0x0007FC00, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{-1, 1, -1}, 0x0007FC00, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, 1, 1}, 0x0007FC00, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{1, 1, 1}, 0x0007FC00, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// +Z
				{{1, -1, 1}, 0x1FF00000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{1, 1, 1}, 0x1FF00000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, 1, 1}, 0x1FF00000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, -1, 1}, 0x1FF00000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// -X
				{{-1, -1, 1}, 0x000003FF, 0x0007FC00, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{-1, 1, 1}, 0x000003FF, 0x0007FC00, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, 1, -1}, 0x000003FF, 0x0007FC00, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, -1, -1}, 0x000003FF, 0x0007FC00, {0.0f, 0.0f}, 0xFFFFFFFF},

				// -Y
				{{-1, -1, -1}, 0x00000000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{1, -1, -1}, 0x00000000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{1, -1, 1}, 0x00000000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, -1, 1}, 0x00000000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// +X
				{{1, -1, -1}, 0x000FFC00, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{1, 1, -1}, 0x000FFC00, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{1, 1, 1}, 0x000FFC00, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{1, -1, 1}, 0x000FFC00, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// -Z
				{{-1, -1, -1}, 0x3FF00000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{-1, 1, -1}, 0x3FF00000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{1, 1, -1}, 0x3FF00000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{1, -1, -1}, 0x3FF00000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},
			};

			std::vector<uint32_t> placeholderIndexData{
				0, 1, 2, 0, 2, 3, // +Y
				4, 5, 6, 4, 6, 7, // +Z
				8, 9, 10, 8, 10, 11, // -X
				12, 13, 14, 12, 14, 15, // -Y
				16, 17, 18, 16, 18, 19, // +X
				20, 21, 22, 20, 22, 23 // -Z
			};

			m_Meshes.EmplaceAt(m_PlaceholderMesh.GetIndex());

			uint32_t indexCount = placeholderIndexData.size();
			AABB3D aabb;

			float bxMax = std::numeric_limits<float>::lowest();
			float byMax = std::numeric_limits<float>::lowest();
			float bzMax = std::numeric_limits<float>::lowest();
			float bxMin = std::numeric_limits<float>::max();
			float byMin = std::numeric_limits<float>::max();
			float bzMin = std::numeric_limits<float>::max();

			for (const uint32_t index : placeholderIndexData) {
				const auto& vertex = placeholderVertexData[index];
				bxMax = std::max(bxMax, vertex.position.x);
				byMax = std::max(byMax, vertex.position.y);
				bzMax = std::max(bzMax, vertex.position.z);
				bxMin = std::min(bxMin, vertex.position.x);
				byMin = std::min(byMin, vertex.position.y);
				bzMin = std::min(bzMin, vertex.position.z);
			}

			aabb.bxCenter = (bxMax + bxMin) * 0.5f;
			aabb.byCenter = (byMax + byMin) * 0.5f;
			aabb.bzCenter = (bzMax + bzMin) * 0.5f;

			aabb.bxExtent = (bxMax - bxMin) * 0.5f;
			aabb.byExtent = (byMax - byMin) * 0.5f;
			aabb.bzExtent = (bzMax - bzMin) * 0.5f;

			auto [success, indexOffset, ticket] = LoadToMesh<StaticVertex>(m_PlaceholderMesh, std::move(placeholderVertexData), std::move(placeholderIndexData), 0, aabb);
			CORI_CORE_ASSERT(success, "Failed to load placeholder mesh.");
			if (!ticket) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "The load of placeholder was rejected by the streaming like due to backpressure, it was queued and will be loaded a bit later. AssignPlaceholder calls will assign an empty mesh in that window.");
			} else {
				VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket.value(), vk::PipelineStageFlagBits::eAllCommands);
				auto& placeholder = m_Meshes[m_PlaceholderMesh];
				placeholder.indexCount = indexCount;
				placeholder.firstIndex = indexOffset / sizeof(uint32_t);
			}
		}

		VulkanMeshManager::~VulkanMeshManager() {
			auto& placeholder = m_MeshMetadata[m_PlaceholderMesh.GetIndex()];
			DeletionQueue::PushVirtualAlloc(placeholder.indexAllocation, m_IndexBufferBlock);
			DeletionQueue::PushVirtualAlloc(placeholder.vertexAllocation, placeholder.vertexStorage->m_Block);
			DeletionQueue::PushVirtualBlock(m_IndexBufferBlock);

			for (auto& vertexStorage : m_VertexStorages) {
				DeletionQueue::PushBuffer(vertexStorage.m_Buffer);
				DeletionQueue::PushVirtualBlock(vertexStorage.m_Block);
			}

			DeletionQueue::PushBuffer(m_IndexBuffer);
		}

		void VulkanMeshManager::Init() {
			CORI_CORE_ASSERT(!s_Instance, "VulkanShaderManager is already initialized.")
			new VulkanMeshManager();
		}

		void VulkanMeshManager::Shutdown() {
			s_Instance.reset();
		}

		VulkanMeshManager& VulkanMeshManager::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling VulkanMeshManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void VulkanMeshManager::BindAsset(const Core::Handle<Mesh> handle, const Core::AssetID id, const uint32_t vectorKey) {
			Get().m_HandleAllocator.BindAsset(handle, id, vectorKey);
		}

		uint32_t VulkanMeshManager::BumpGeneration(const Core::Handle<Mesh> handle) {
			return Get().m_HandleAllocator.BumpGeneration(handle);
		}

		bool VulkanMeshManager::IsHandleValid(const Core::Handle<Mesh> handle) {
			return Get().IsHandleValidImpl(handle);
		}

		Core::AssetID VulkanMeshManager::GetAssetID(const Core::Handle<Mesh> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to GetAssetID in VulkanMeshManager is invalid.");
			return Get().m_HandleAllocator.GetBoundAssetID(handle);
		}

		bool VulkanMeshManager::TryAddRef(const Core::Handle<Mesh> handle) {
			return Get().m_HandleAllocator.TryAddRef(handle);
		}

		void VulkanMeshManager::AddRef(Core::Handle<Mesh> handle) {
			Get().m_HandleAllocator.AddRef(handle);
		}

		void VulkanMeshManager::RemoveRef(Core::Handle<Mesh> handle) {
			Get().m_HandleAllocator.RemoveRef(handle);
		}

		void VulkanMeshManager::SetAssetStatus(const Core::Handle<Mesh> handle, const AssetStatus newStatus) {
			Get().SetAssetStatusImpl(handle, newStatus);
		}

		AssetStatus VulkanMeshManager::GetAssetStatus(const Core::Handle<Mesh> handle) {
			return Get().m_HandleAllocator.GetAssetStatus(handle);
		}

		uint32_t VulkanMeshManager::GetIdentityVersion(const Core::Handle<Mesh> handle) {
			return Get().m_HandleAllocator.GetIdentityVersion(handle);
		}

		std::expected<std::pair<Core::AssetDependencySet, uint32_t>, ErrorCode> VulkanMeshManager::TryReadDependencies(const Core::Handle<Mesh> handle) {
			return Get().m_HandleAllocator.TryReadDependencies(handle);
		}

		void VulkanMeshManager::PublishIdentity(const Core::Handle<Mesh> handle) {
			Get().m_HandleAllocator.PublishIdentity(handle, Core::MakeAssetDependencySet(std::as_const(Get().m_Meshes)[handle]));
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

							Get().LoadToMesh(handle_, std::move(std::get<std::vector<StaticVertex>>(payload.m_VertexData)), std::move(payload.m_IndexData), gen_, payload.m_AABB, payload.m_CompleteVertexAlloc);

							auto& aabb = Get().m_AABBs[handle_.GetIndex()];
							aabb.gen.fetch_add(1, std::memory_order_relaxed);
							std::atomic_thread_fence(std::memory_order_release);

							aabb.bxCenter.store(std::bit_cast<uint32_t>(payload.m_AABB.bxCenter), std::memory_order_relaxed);
							aabb.byCenter.store(std::bit_cast<uint32_t>(payload.m_AABB.byCenter), std::memory_order_relaxed);
							aabb.bzCenter.store(std::bit_cast<uint32_t>(payload.m_AABB.bzCenter), std::memory_order_relaxed);
							aabb.bxExtent.store(std::bit_cast<uint32_t>(payload.m_AABB.bxExtent), std::memory_order_relaxed);
							aabb.byExtent.store(std::bit_cast<uint32_t>(payload.m_AABB.byExtent), std::memory_order_relaxed);
							aabb.bzExtent.store(std::bit_cast<uint32_t>(payload.m_AABB.bzExtent), std::memory_order_relaxed);

							aabb.gen.fetch_add(1, std::memory_order_release);

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

					AABB3D aabb;

					float bxMax = std::numeric_limits<float>::lowest();
					float byMax = std::numeric_limits<float>::lowest();
					float bzMax = std::numeric_limits<float>::lowest();
					float bxMin = std::numeric_limits<float>::max();
					float byMin = std::numeric_limits<float>::max();
					float bzMin = std::numeric_limits<float>::max();

					for (const uint32_t index : indexData) {
						const auto& vertex = vertexData[index];
						bxMax = std::max(bxMax, vertex.position.x);
						byMax = std::max(byMax, vertex.position.y);
						bzMax = std::max(bzMax, vertex.position.z);
						bxMin = std::min(bxMin, vertex.position.x);
						byMin = std::min(byMin, vertex.position.y);
						bzMin = std::min(bzMin, vertex.position.z);
					}

					aabb.bxCenter = (bxMax + bxMin) * 0.5f;
					aabb.byCenter = (byMax + byMin) * 0.5f;
					aabb.bzCenter = (bzMax + bzMin) * 0.5f;

					aabb.bxExtent = (bxMax - bxMin) * 0.5f;
					aabb.byExtent = (byMax - byMin) * 0.5f;
					aabb.bzExtent = (bzMax - bzMin) * 0.5f;

					auto payload = WorkerPayload(std::move(vertexData), std::move(indexData), ca);
					payload.m_AABB = aabb;

					FinalizeLoad(handle, id, gen, vectorKey, std::move(payload));
				}
			});
		}

		void VulkanMeshManager::Unload(const Core::Handle<Mesh> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UNLOAD (destroying mesh, freeing handle)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());
			CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
			CORI_CORE_ASSERT(handle != Get().m_PlaceholderMesh, "Placeholder mesh handle was passed, can't unload it.")

			Core::AssetID id = Get().m_HandleAllocator.GetBoundAssetID(handle);
			{
				auto& mutex = Core::AssetManager2::GetMutex();
				std::lock_guard lk(mutex);
				CORI_PROFILER_LOCK_MARK(mutex);
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				uint32_t vectorKey = record.vectorKey;
				auto old = Core::Handle<Mesh>(Core::AssetManager2::GetRawHandle(vectorKey));
				if (old == handle) {
					Core::AssetManager2::SetRawHandle(vectorKey, Core::VersionedHandleBase::Null);
				}
			}

			Get().m_HandleAllocator.Free(handle);

			if (!Get().m_Meshes.IsIndexOccupied(handle.GetIndex())) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, no mesh slot was occupied");
				return;
			}

			Get().DestroyMesh(handle);
			Get().m_Meshes.RemoveAt(handle.GetIndex());
			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, mesh destroyed and slot released");
		}

		void VulkanMeshManager::QueueUnload(const Core::Handle<Mesh> handle) {
			RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
		}

		void VulkanMeshManager::ProcessUpdates(vk::CommandBuffer cmb) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Process);
			Get().m_BarrierCache.clear();
			auto currentTimelineValue = VulkanStreamingLine::GetTimelineValue();

			for (auto& [ticket, inTransferAssets] : Get().m_MeshesInTransfer) {
				if (currentTimelineValue >= ticket) {
					for (auto& inTransferMesh : inTransferAssets) {
						CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "ProcessUpdates: Mesh transfer complete", Cori::ProfileColors::Loaded);
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion());
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Ticket=%llu, Current timeline value=%llu", static_cast<unsigned long long>(ticket), static_cast<unsigned long long>(currentTimelineValue));
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Indices=%u @ firstIndex=%u, vertices=%u bytes @ storage=%p +%u", inTransferMesh.indexCount, inTransferMesh.indexOffset, inTransferMesh.vertexByteSize, static_cast<void*>(inTransferMesh.vertexStorage), inTransferMesh.vertexByteOffset);

						if (!IsHandleValid(inTransferMesh.mesh)) {
							CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, freed vertex/index allocations");
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] transfer done but handle invalid, freed vertex/index allocations (ticket %llu)", CORI_CLEAN_TYPE_NAME(Mesh), inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion(), static_cast<unsigned long long>(ticket));
							DeletionQueue::PushVirtualAlloc(inTransferMesh.vertexAllocation, inTransferMesh.vertexStorage->m_Block, inTransferMesh.vertexStorage->m_Mutex);
							DeletionQueue::PushVirtualAlloc(inTransferMesh.indexAllocation, Get().m_IndexBufferBlock);
							inTransferMesh.vertexStorage->RemoveStrongRef();
							inTransferMesh.vertexStorage->RemoveWeakRef();
							continue;
						}

						if (Get().m_HandleAllocator.GetGeneration(inTransferMesh.mesh) != inTransferMesh.loadGen) {
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (loadGen=%u, current gen=%u), freed vertex/index allocations", inTransferMesh.loadGen, Get().m_HandleAllocator.GetGeneration(inTransferMesh.mesh));
							CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] transfer STALE (loadGen=%u, current gen=%u) (ticket %llu), freed vertex/index allocations", CORI_CLEAN_TYPE_NAME(Mesh), inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion(), inTransferMesh.loadGen, Get().m_HandleAllocator.GetGeneration(inTransferMesh.mesh), static_cast<unsigned long long>(ticket));
							DeletionQueue::PushVirtualAlloc(inTransferMesh.vertexAllocation, inTransferMesh.vertexStorage->m_Block, inTransferMesh.vertexStorage->m_Mutex);
							DeletionQueue::PushVirtualAlloc(inTransferMesh.indexAllocation, Get().m_IndexBufferBlock);
							inTransferMesh.vertexStorage->RemoveStrongRef();
							inTransferMesh.vertexStorage->RemoveWeakRef();
							continue;
						}

						inTransferMesh.vertexStorage->RemoveStrongRef();

						auto& meshData = Get().m_Meshes[inTransferMesh.mesh];
						meshData.indexCount = inTransferMesh.indexCount;
						meshData.firstIndex = inTransferMesh.indexOffset;

						auto& meta = Get().m_MeshMetadata[inTransferMesh.mesh.GetIndex()];
						meta.loaded = true;
						PublishIdentity(inTransferMesh.mesh);
						SetAssetStatus(inTransferMesh.mesh, AssetStatus::eLoaded);
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: LOADED, real mesh now drawable (indexCount=%u, firstIndex=%u, firstVertexAddress=0x%llx), storage refs strong=%u weak=%u", meshData.indexCount, meshData.firstIndex, static_cast<unsigned long long>(meshData.firstVertexAddress), inTransferMesh.vertexStorage->GetStrongRefCount(), inTransferMesh.vertexStorage->GetWeakRefCount());
						CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded, "%s Handle=[%u, %u] LOADED, real mesh now drawable (indexCount=%u, firstIndex=%u) (ticket %llu)", CORI_CLEAN_TYPE_NAME(Mesh), inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion(), meshData.indexCount, meshData.firstIndex, static_cast<unsigned long long>(ticket));
					}

					VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket, vk::PipelineStageFlagBits::eVertexShader);

					inTransferAssets.clear();
				}
			}

			if (!Get().m_BarrierCache.empty()) {
				vk::DependencyInfo depInfo{
					.bufferMemoryBarrierCount = static_cast<uint32_t>(Get().m_BarrierCache.size()),
					.pBufferMemoryBarriers = Get().m_BarrierCache.data()
				};

				cmb.pipelineBarrier2(depInfo);
			}

			while (!Get().m_QueuedUploads.empty()) {
				auto& upload = Get().m_QueuedUploads.front();
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "ProcessUpdates: Queued upload retry", Cori::ProfileColors::Upload);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", upload.mesh.GetIndex(), upload.mesh.GetVersion());
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Indices=%llu @ byte offset %llu, vertex storage=%p @ byte offset %llu", static_cast<unsigned long long>(upload.indexData.size()), static_cast<unsigned long long>(upload.indexUpload.range.offset), static_cast<void*>(upload.vertexStorage), static_cast<unsigned long long>(upload.vertexUpload.range.offset));

				if (!IsHandleValid(upload.mesh)) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, queued upload dropped and allocations freed");
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] queued upload dropped (handle invalid) (gen=%u)", CORI_CLEAN_TYPE_NAME(Mesh), upload.mesh.GetIndex(), upload.mesh.GetVersion(), upload.loadGen);
					{
						std::lock_guard lk(*upload.vertexStorage->m_Mutex);
						upload.vertexStorage->m_Block.free(upload.vertexAlloc);
					}
					Get().m_IndexBufferBlock.free(upload.indexAlloc);
					upload.vertexStorage->RemoveStrongRef();
					upload.vertexStorage->RemoveWeakRef();
					Get().m_QueuedUploads.pop();
					continue;
				}

				if (upload.loadGen != Get().m_HandleAllocator.GetGeneration(upload.mesh)) {
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (loadGen=%u, current gen=%u), queued upload dropped and allocations freed", upload.loadGen, Get().m_HandleAllocator.GetGeneration(upload.mesh));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] queued upload dropped (STALE loadGen=%u, current gen=%u)", CORI_CLEAN_TYPE_NAME(Mesh), upload.mesh.GetIndex(), upload.mesh.GetVersion(), upload.loadGen, Get().m_HandleAllocator.GetGeneration(upload.mesh));
					{
						std::lock_guard lk(*upload.vertexStorage->m_Mutex);
						upload.vertexStorage->m_Block.free(upload.vertexAlloc);
					}
					Get().m_IndexBufferBlock.free(upload.indexAlloc);
					upload.vertexStorage->RemoveStrongRef();
					upload.vertexStorage->RemoveWeakRef();
					Get().m_QueuedUploads.pop();
					continue;
				}

				std::array<VulkanStreamingLine::GenericUpload, 2> requests;

				requests[0] = {
					.resourceUpload = upload.vertexUpload,
				};

				uint32_t vertexDataSize = 0;
				if (std::holds_alternative<std::vector<StaticVertex>>(upload.vertexData)) {
					requests[0].data = std::span<Byte>(reinterpret_cast<Byte*>(std::get<std::vector<StaticVertex>>(upload.vertexData).data()), std::get<std::vector<StaticVertex>>(upload.vertexData).size() * sizeof(StaticVertex));
					vertexDataSize = std::get<std::vector<StaticVertex>>(upload.vertexData).size() * sizeof(StaticVertex);
				}

				requests[1] = {
					.resourceUpload = upload.indexUpload,
					.data = std::span<Byte>(reinterpret_cast<Byte*>(upload.indexData.data()), upload.indexData.size() * sizeof(uint32_t))
				};

				auto& meta = Get().m_MeshMetadata[upload.mesh.GetIndex()];
				auto result = VulkanStreamingLine::SubmitUploads(requests);

				if (result) {
					Get().FindInTransferSlot(result.value()).emplace_back(MeshInTransfer{
						.mesh = upload.mesh,
						.vertexStorageBuffer = upload.vertexStorage->m_Buffer.m_Buffer,
						.vertexAllocation = upload.vertexAlloc,
						.vertexStorage = upload.vertexStorage,
						.indexAllocation = upload.indexAlloc,
						.indexCount = static_cast<uint32_t>(upload.indexData.size()),
						.indexOffset = static_cast<uint32_t>(upload.indexUpload.range.offset / sizeof(uint32_t)),
						.vertexByteSize = vertexDataSize,
						.vertexByteOffset = static_cast<uint32_t>(upload.vertexUpload.range.offset),
						.loadGen = upload.loadGen,
					});

					SetAssetStatus(upload.mesh, AssetStatus::eStreaming);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: SUBMITTED (ticket=%llu), status=Streaming", static_cast<unsigned long long>(result.value()));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] queued upload SUBMITTED (ticket=%llu) (gen=%u), status=Streaming", CORI_CLEAN_TYPE_NAME(Mesh), upload.mesh.GetIndex(), upload.mesh.GetVersion(), static_cast<unsigned long long>(result.value()), upload.loadGen);

					Get().m_QueuedUploads.pop();
				} else {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line still full, retry deferred to next frame");
					break;
				}
			}

			Get().m_Meshes.Sync();
		}

		VulkanBuffer& VulkanMeshManager::GetIndexBuffer() {
			return Get().m_IndexBuffer;
		}

		uint64_t VulkanMeshManager::GetMeshAssetBufferBDA() {
			return Get().m_Meshes.GetVulkanBuffer().GetBDA();
		}

		std::pair<uint32_t, uint32_t> VulkanMeshManager::GetDrawRange(const Core::Handle<Mesh> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			const auto& mesh = std::as_const(Get().m_Meshes)[handle];
			return { mesh.indexCount, mesh.firstIndex };
		}

		std::expected<AABB3D, ErrorCode> VulkanMeshManager::GetAABB3D(const Core::Handle<Mesh> handle) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle.");

			AABBAtomics& atomics = Get().m_AABBs[handle.GetIndex()];
			uint32_t gen0 = atomics.gen.load(std::memory_order_acquire);
			uint32_t gen1 = 0;
			if (gen0 == 0) {
				return std::unexpected(ErrorCode::eNotReady);
			}

			AABB3D aabb;

			while (gen0 != gen1) {
				gen0 = atomics.gen.load(std::memory_order_acquire);
				if ((gen0 & 1u) != 0) {
					continue;
				}

				aabb.bxCenter = std::bit_cast<float>(atomics.bxCenter.load(std::memory_order_relaxed));
				aabb.byCenter = std::bit_cast<float>(atomics.byCenter.load(std::memory_order_relaxed));
				aabb.bzCenter = std::bit_cast<float>(atomics.bzCenter.load(std::memory_order_relaxed));
				aabb.bxExtent = std::bit_cast<float>(atomics.bxExtent.load(std::memory_order_relaxed));
				aabb.byExtent = std::bit_cast<float>(atomics.byExtent.load(std::memory_order_relaxed));
				aabb.bzExtent = std::bit_cast<float>(atomics.bzExtent.load(std::memory_order_relaxed));

				std::atomic_thread_fence(std::memory_order_acquire);
				gen1 = atomics.gen.load(std::memory_order_relaxed);
			}

			return aabb;
		}

		void VulkanMeshManager::AllocateExtras(const Core::Handle<Mesh> handle) {
			const uint64_t newSizePowerOfTwo = Utility::GetNextPowerOfTwo(handle.GetIndex() + 1);

			if (newSizePowerOfTwo >= Get().m_AABBs.size()) {
				Get().m_AABBs.grow_to_at_least(newSizePowerOfTwo);
			}
		}

		void VulkanMeshManager::FreeExtras(const Core::Handle<Mesh> handle) {
			Get().m_AABBs[handle.GetIndex()].gen.store(0, std::memory_order_release);
		}

		bool VulkanMeshManager::IsHandleValidImpl(const Core::Handle<Mesh> handle) const {
			return m_HandleAllocator.IsHandleValid(handle);
		}

		void VulkanMeshManager::SetAssetStatusImpl(const Core::Handle<Mesh> handle, const AssetStatus newStatus) {
			m_HandleAllocator.SetAssetStatus(handle, newStatus);
		}

		void VulkanMeshManager::AssignPlaceholder(const Core::Handle<Mesh> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
			CORI_CORE_ASSERT(IsHandleValidImpl(handle), "Invalid handle.");

			auto placeholderData = std::as_const(m_Meshes)[m_PlaceholderMesh];
			placeholderData.version = handle.GetVersion();
			m_Meshes[handle] = placeholderData;

			m_MeshMetadata[handle.GetIndex()].placeholderAssigned = true;

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> PLACEHOLDER cube mesh (indexCount=%u, firstIndex=%u)", handle.GetIndex(), handle.GetVersion(), placeholderData.indexCount, placeholderData.firstIndex);
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned PLACEHOLDER cube mesh (indexCount=%u, firstIndex=%u)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), placeholderData.indexCount, placeholderData.firstIndex);
		}

		void VulkanMeshManager::AssignEmptyMesh(const Core::Handle<Mesh> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
			CORI_CORE_ASSERT(IsHandleValidImpl(handle), "Invalid handle.");

			m_Meshes[handle] = Mesh{ .version = handle.GetVersion() };
			m_MeshMetadata[handle.GetIndex()].placeholderAssigned = true;

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> PLACEHOLDER empty mesh (indexCount=0, nothing drawn)", handle.GetIndex(), handle.GetVersion());
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned PLACEHOLDER empty mesh (indexCount=0, nothing drawn)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());
		}

		std::pair<uint32_t, uint32_t> VulkanMeshManager::EnsureMetadataSlot(const uint32_t index) {
			const auto size = static_cast<uint32_t>(m_MeshMetadata.size());
			if (index < size) {
				return { size, size };
			}

			m_MeshMetadata.resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
			return { size, static_cast<uint32_t>(m_MeshMetadata.size()) };
		}

		template<typename VertexT> requires std::same_as<VertexT, StaticVertex>
		std::tuple<bool, vk::DeviceSize, std::optional<uint64_t>> VulkanMeshManager::LoadToMesh(const Core::Handle<Mesh> handle, std::vector<VertexT>&& vertices, std::vector<uint32_t>&& indices, const uint32_t loadGen, const AABB3D& aabb, const std::optional<CompleteVertexAllocation>& completeVertexAlloc) {
			constexpr VertexType vertexType = []{
				if constexpr (std::is_same_v<VertexT, StaticVertex>) {
					return VertexType::eStatic;
				}
			}();

			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Upload);
			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] loadGen=%u vertices=%llu (%llu bytes) indices=%llu (%llu bytes)", handle.GetIndex(), handle.GetVersion(), loadGen, static_cast<unsigned long long>(vertices.size()), static_cast<unsigned long long>(vertices.size() * sizeof(VertexT)), static_cast<unsigned long long>(indices.size()), static_cast<unsigned long long>(indices.size() * sizeof(uint32_t)));
			CORI_CORE_ASSERT(IsHandleValidImpl(handle), "Invalid handle.");

			#ifdef CORI_VALIDATION_LAYER
			CORI_CORE_ASSERT(indices.size() % 3 == 0, "Mesh [{}, {}] tried loading with {} indices, which is not a whole number of triangles.", handle.GetIndex(), handle.GetVersion(), indices.size());

			for (uint64_t i = 0; i < indices.size(); i++) {
				if (indices[i] >= vertices.size()) {
					CORI_CORE_ASSERT(false, "Mesh [{}, {}] index {} of {} has value {}, out of range for a mesh with {} vertices. The vertex shader would fetch {} bytes past the start of this mesh's vertex block.", handle.GetIndex(), handle.GetVersion(), i, indices.size(), indices[i], vertices.size(), static_cast<uint64_t>(indices[i]) * sizeof(VertexT));
					break;
				}
			}
			#endif

			vma::VirtualAllocationCreateInfo indicesAllocInfo {
				.size = indices.size() * sizeof(uint32_t),
				.alignment = alignof(uint32_t),
				.flags = s_IndexAllocFlags
			};

			vma::VirtualAllocationCreateInfo verticesAllocInfo {
				.size = vertices.size() * sizeof(VertexT),
				.alignment = alignof(VertexT),
				.flags = s_VertexAllocFlags
			};

			vk::DeviceSize indexOffset;
			auto [result, indexAlloc] = m_IndexBufferBlock.virtualAllocate(indicesAllocInfo, indexOffset);
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new indices, error: {}", vk::to_string(result));

			vk::DeviceSize vertexOffset;
			vma::VirtualAllocation vertexAlloc;
			VertexStorage* vertexStorage = nullptr;

			if (!completeVertexAlloc) {
				vertexStorage = AllocateNewVertexStorage();
				vertexStorage->DiscoverBoth();

				std::lock_guard lk(*vertexStorage->m_Mutex);
				auto [result2, alloc] = vertexStorage->m_Block.virtualAllocate(verticesAllocInfo, vertexOffset);
				CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new vertices in a newly created vertex storage, error: {}", vk::to_string(result2));
				vertexAlloc = alloc;
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Vertex alloc: NEW storage %p @ byte offset %llu, index alloc @ byte offset %llu", static_cast<void*>(vertexStorage), static_cast<unsigned long long>(vertexOffset), static_cast<unsigned long long>(indexOffset));
			} else {
				const auto& cva = completeVertexAlloc.value();
				vertexOffset = cva.offset;
				vertexAlloc = cva.allocation;
				vertexStorage = cva.storage;
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Vertex alloc: reused worker allocation in storage %p @ byte offset %llu, index alloc @ byte offset %llu", static_cast<void*>(vertexStorage), static_cast<unsigned long long>(vertexOffset), static_cast<unsigned long long>(indexOffset));
			}

			VulkanStreamingLine::BufferUpload vertexUpload{
				.resource = vertexStorage->m_Buffer,
				.range = {.offset = vertexOffset, .alignment = alignof(VertexT) },
				.srcPipelineStages = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessFlags = vk::AccessFlagBits2::eNone
			};

			VulkanStreamingLine::BufferUpload indexUpload{
				.resource = m_IndexBuffer,
				.range = {.offset = indexOffset, .alignment = alignof(uint32_t) },
				.srcPipelineStages = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessFlags = vk::AccessFlagBits2::eNone
			};

			std::array<VulkanStreamingLine::GenericUpload, 2> uploads;

			uploads[0] = {
				.resourceUpload = vertexUpload,
				.data = std::span<const Byte>(reinterpret_cast<const Byte*>(vertices.data()), vertices.size() * sizeof(VertexT))
			};

			uploads[1] = {
				.resourceUpload = indexUpload,
				.data = std::span<const Byte>(reinterpret_cast<const Byte*>(indices.data()), indices.size() * sizeof(uint32_t))
			};

			auto streamingResult = VulkanStreamingLine::SubmitUploads(uploads);

			if (streamingResult) {
				Mesh mesh{
					.indexCount = 0,
					.firstIndex = 0,
					.firstVertexAddress = vertexStorage->m_Buffer.GetBDA() + vertexOffset,
					.vertexType = std::to_underlying(vertexType),
					.version = handle.GetVersion(),
					.bxCenter = aabb.bxCenter,
					.byCenter = aabb.byCenter,
					.bzCenter = aabb.bzCenter,
					.bxExtent = aabb.bxExtent,
					.byExtent = aabb.byExtent,
					.bzExtent = aabb.bzExtent
				};

				m_Meshes[handle] = mesh;

				auto& meta = m_MeshMetadata[handle.GetIndex()];

				meta.vertexAllocation = vertexAlloc;
				meta.vertexStorage = vertexStorage;
				meta.indexAllocation = indexAlloc;

				SetAssetStatusImpl(handle, AssetStatus::eStreaming);

				FindInTransferSlot(streamingResult.value()).emplace_back(MeshInTransfer{
					.mesh = handle,
					.vertexStorageBuffer = vertexStorage->m_Buffer.m_Buffer,
					.vertexAllocation = meta.vertexAllocation,
					.vertexStorage = meta.vertexStorage,
					.indexAllocation = meta.indexAllocation,
					.indexCount = static_cast<uint32_t>(indices.size()),
					.indexOffset = static_cast<uint32_t>(indexOffset / sizeof(uint32_t)),
					.vertexByteSize = static_cast<uint32_t>(vertices.size() * sizeof(VertexT)),
					.vertexByteOffset =  static_cast<uint32_t>(vertexOffset),
					.loadGen = loadGen
				});

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Upload SUBMITTED to streaming line (ticket=%llu), status=Streaming", static_cast<unsigned long long>(streamingResult.value()));
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] upload SUBMITTED to streaming line (ticket=%llu, indices=%llu, vertexBytes=%llu), status=Streaming", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), static_cast<unsigned long long>(streamingResult.value()), static_cast<unsigned long long>(indices.size()), static_cast<unsigned long long>(vertices.size() * sizeof(VertexT)));

				return { true, indexOffset, streamingResult.value() };
			}

			if (streamingResult.error() == ErrorCode::eInvalidData) {
				{
					std::lock_guard lk(*vertexStorage->m_Mutex);
					vertexStorage->m_Block.free(vertexAlloc);
				}
				vertexStorage->RemoveStrongRef();
				vertexStorage->RemoveWeakRef();
				m_IndexBufferBlock.free(indexAlloc);
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "VulkanStreamingLine returned with error code eInvalidData during CreateMesh call, using placeholder.");

				AssignPlaceholder(handle);

				SetAssetStatusImpl(handle, AssetStatus::eLoadFailed);

				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line returned eInvalidData, PLACEHOLDER assigned, status=LoadFailed");
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LoadToMesh FAILED: streaming line eInvalidData, PLACEHOLDER assigned, status=LoadFailed", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());

				return { false, indexOffset, std::nullopt };
			}

			Mesh mesh{
				.indexCount = 0,
				.firstIndex = 0,
				.firstVertexAddress = vertexStorage->m_Buffer.GetBDA() + vertexOffset,
				.vertexType = std::to_underlying(vertexType),
				.version = handle.GetVersion(),
				.bxCenter = aabb.bxCenter,
				.byCenter = aabb.byCenter,
				.bzCenter = aabb.bzCenter,
				.bxExtent = aabb.bxExtent,
				.byExtent = aabb.byExtent,
				.bzExtent = aabb.bzExtent
			};

			m_Meshes[handle] = mesh;

			auto& meta = m_MeshMetadata[handle.GetIndex()];

			meta.vertexAllocation = vertexAlloc;
			meta.vertexStorage = vertexStorage;
			meta.indexAllocation = indexAlloc;

			SetAssetStatusImpl(handle, AssetStatus::eStreamingQueued);

			CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line full, upload QUEUED, status=StreamingQueued (still showing placeholder)");
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] upload QUEUED (streaming line full), status=StreamingQueued", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());

			m_QueuedUploads.emplace(QueuedUpload{
				.vertexUpload = vertexUpload,
				.indexUpload = indexUpload,
				.vertexData = std::move(vertices),
				.indexData = std::move(indices),
				.mesh = handle,
				.vertexStorage = vertexStorage,
				.indexAlloc = indexAlloc,
				.vertexAlloc = vertexAlloc,
				.loadGen = loadGen
			});

			CORI_DEBUG("Queued");
			counter++;
			return { true, indexOffset, std::nullopt };
		}

		VulkanMeshManager::VertexStorage* VulkanMeshManager::AllocateNewVertexStorage() {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Alloc);
			auto& sharingSettings = VulkanEngine::GetBufferSharingSettings(BUFFER_USAGE);
			vk::BufferCreateInfo vkBufferInfo{
				.size = VERTEX_STORAGE_SIZE,
				.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
				.sharingMode = sharingSettings.first,
				.queueFamilyIndexCount = static_cast<uint32_t>(sharingSettings.second.size()),
				.pQueueFamilyIndices = sharingSettings.second.data()
			};

			vma::AllocationCreateInfo bufferAllocInfo{
				.flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
				.usage = vma::MemoryUsage::eAuto
			};

			VulkanBuffer::CreateInfo createInfo {
				.bufferCreateInfo = &vkBufferInfo,
				.allocationCreateInfo = &bufferAllocInfo
			};

			#ifdef DEBUG_BUILD
			std::string name = std::format("Mesh Manager vertex storage buffer {}", m_VertexStorageBufferCounter.fetch_add(1, std::memory_order_relaxed));
			createInfo.name = name.c_str();
			#endif

			vma::VirtualBlockCreateInfo vertexBlockCreateInfo{
				.size = VERTEX_STORAGE_SIZE
			};

			auto [result1, vertexBlock] = vma::createVirtualBlock(vertexBlockCreateInfo);
			CORI_CORE_ASSERT(result1 == vk::Result::eSuccess, "Failed to create vma vertex storage virtual block. Error: {}", vk::to_string(result1));

			auto storage = m_VertexStorages.emplace_back(VulkanBuffer::Create(createInfo), vertexBlock);

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "New vertex storage %p (%llu bytes), total storages=%llu", static_cast<void*>(&*storage), static_cast<unsigned long long>(VERTEX_STORAGE_SIZE), static_cast<unsigned long long>(m_VertexStorages.size()));
			CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Alloc, "%s vertex storage ALLOCATED %p (%llu bytes), total storages=%llu", CORI_CLEAN_TYPE_NAME(Mesh), static_cast<void*>(&*storage), static_cast<unsigned long long>(VERTEX_STORAGE_SIZE), static_cast<unsigned long long>(m_VertexStorages.size()));

			return &*storage;
		}

		void VulkanMeshManager::DestroyMesh(Core::Handle<Mesh> handle) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
			if (handle == m_PlaceholderMesh) {
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Skipped: placeholder mesh is never destroyed");
				return;
			}

			auto& meta = m_MeshMetadata[handle.GetIndex()];

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] (placeholderAssigned=%d, loaded=%d)", handle.GetIndex(), handle.GetVersion(), static_cast<int>(meta.placeholderAssigned), static_cast<int>(meta.loaded));
			CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] DestroyMesh (placeholderAssigned=%d, loaded=%d)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), static_cast<int>(meta.placeholderAssigned), static_cast<int>(meta.loaded));

			if (!meta.placeholderAssigned && meta.loaded) {
				DeletionQueue::PushVirtualAlloc(meta.vertexAllocation, meta.vertexStorage->m_Block, meta.vertexStorage->m_Mutex);
				DeletionQueue::PushVirtualAlloc(meta.indexAllocation, Get().m_IndexBufferBlock);
				meta.vertexStorage->RemoveWeakRef();
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Queued vertex/index allocations for deletion (storage=%p, refs now strong=%u weak=%u)", static_cast<void*>(meta.vertexStorage), meta.vertexStorage->GetStrongRefCount(), meta.vertexStorage->GetWeakRefCount());
			}

			meta.loaded = false;
			meta.placeholderAssigned = false;

			auto& meshData = m_Meshes[handle];
			meshData.indexCount = 0;
			meshData.firstIndex = 0;
			meshData.firstVertexAddress = 0;
			meshData.vertexType = 0;
			meshData.bxCenter = 0.0f;
			meshData.byCenter = 0.0f;
			meshData.bzCenter = 0.0f;
			meshData.bxExtent = 0.0f;
			meshData.byExtent = 0.0f;
			meshData.bzExtent = 0.0f;
		}

		std::vector<VulkanMeshManager::MeshInTransfer>& VulkanMeshManager::FindInTransferSlot(const uint64_t value) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Upload);
			InTransferSlot* free = nullptr;
			InTransferSlot* bestPick = nullptr;

			for (auto& pair : m_MeshesInTransfer) {
				if (pair.ticket == value) {
					bestPick = &pair;
					break;
				}

				if (pair.meshesInTransfer.empty()) {
					free = &pair;
				}
			}

			if (bestPick) {
				bestPick->ticket = value;
				return bestPick->meshesInTransfer;
			}

			CORI_CORE_ASSERT(free, "Failed to find any free in transfer slot for a mesh.");

			free->ticket = value;
			return free->meshesInTransfer;
		}

		uint32_t VulkanMeshManager::PackNormal(const glm::vec3& n) {
			glm::vec3 norm = glm::clamp(n, -1.0f, 1.0f);
			uint32_t x = (static_cast<int32_t>(std::round(norm.x * 511.0f)) & 0x3FF);
			uint32_t y = (static_cast<int32_t>(std::round(norm.y * 511.0f)) & 0x3FF);
			uint32_t z = (static_cast<int32_t>(std::round(norm.z * 511.0f)) & 0x3FF);
			return x | (y << 10) | (z << 20);
		}

		uint32_t VulkanMeshManager::PackTangent(const glm::vec3& t, float bitangentSign) {
			glm::vec3 norm = glm::clamp(t, -1.0f, 1.0f);
			uint32_t x = (static_cast<int32_t>(std::round(norm.x * 511.0f)) & 0x3FF);
			uint32_t y = (static_cast<int32_t>(std::round(norm.y * 511.0f)) & 0x3FF);
			uint32_t z = (static_cast<int32_t>(std::round(norm.z * 511.0f)) & 0x3FF);
			uint32_t w = (bitangentSign < 0.0f) ? 0 : 1;
			return x | (y << 10) | (z << 20) | (w << 30);
		}

		uint32_t VulkanMeshManager::PackColor(const glm::vec3& c) {
			uint32_t r = static_cast<uint32_t>(glm::clamp(c.r, 0.0f, 1.0f) * 255.0f);
			uint32_t g = static_cast<uint32_t>(glm::clamp(c.g, 0.0f, 1.0f) * 255.0f);
			uint32_t b = static_cast<uint32_t>(glm::clamp(c.b, 0.0f, 1.0f) * 255.0f);
			uint32_t a = 255;
			return r | (g << 8) | (b << 16) | (a << 24);
		}

		bool VulkanMeshManager::LoadObjToEngine(const char* filepath, std::vector<StaticVertex>& outVertices, std::vector<uint32_t>& outIndices) {
			CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Decode);
			fastObjMesh* mesh = nullptr;
			{
				CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "OBJ read (fast_obj)", Cori::ProfileColors::Decode);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "File: %s", filepath);
				mesh = fast_obj_read(filepath);
			}

			if (!mesh) {
				//the handle-scoped LOAD FAILED message is emitted by the caller, which knows the handle
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: fast_obj_read could not read '%s'", filepath);
				std::cerr << "Failed to load obj: " << filepath << std::endl;
				return false;
			}

			std::unordered_map<fastObjIndex, uint32_t, FastObjIndexHash, FastObjIndexEq> uniqueVertices;

			// We store raw/unpacked normals temporarily to compute proper tangents later
			std::vector<glm::vec3> tempNormals;

			size_t index_offset = 0;

			// Phase 1: Vertex deduplication and triangulation
			for (unsigned int i = 0; i < mesh->face_count; ++i) {
				unsigned int num_vertices = mesh->face_vertices[i];

				// Triangulate n-gons (quads, etc.) into triangles using a triangle fan
				for (unsigned int j = 1; j < num_vertices - 1; ++j) {
					fastObjIndex objIndices[3] = {
							mesh->indices[index_offset],
							mesh->indices[index_offset + j],
							mesh->indices[index_offset + j + 1]
						};

					for (int k = 0; k < 3; ++k) {
						const fastObjIndex& idx = objIndices[k];

						if (uniqueVertices.count(idx) == 0) {
							StaticVertex vertex{};
							glm::vec3 normal(0.0f, 1.0f, 0.0f); // Default normal

							// Positions (fast_obj places a dummy at index 0, so valid indices start at 1)
							vertex.position = {
									mesh->positions[idx.p * 3 + 0],
									mesh->positions[idx.p * 3 + 1],
									mesh->positions[idx.p * 3 + 2]
								};

							// Normals
							if (idx.n) {
								normal = {
										mesh->normals[idx.n * 3 + 0],
										mesh->normals[idx.n * 3 + 1],
										mesh->normals[idx.n * 3 + 2]
									};
							}
							vertex.normal = PackNormal(normal);
							tempNormals.push_back(normal); // Save raw normal for tangent math

							// UVs
							if (idx.t) {
								vertex.uv = {
										mesh->texcoords[idx.t * 2 + 0],
										1.0f - mesh->texcoords[idx.t * 2 + 1] // Vulkan/D3D usually flips V
									};
							}
							else {
								vertex.uv = {0.0f, 0.0f};
							}

							// Colors
							glm::vec3 color(1.0f);
							if (mesh->colors && mesh->color_count > idx.p * 3 + 2) {
								color = {
										mesh->colors[idx.p * 3 + 0],
										mesh->colors[idx.p * 3 + 1],
										mesh->colors[idx.p * 3 + 2]
									};
							}
							vertex.color = PackColor(color);
							vertex.tangent = 0; // Filled in Phase 2

							uint32_t newVertexIndex = static_cast<uint32_t>(outVertices.size());
							outVertices.push_back(vertex);
							uniqueVertices[idx] = newVertexIndex;
						}

						outIndices.push_back(uniqueVertices[idx]);
					}
				}
				index_offset += num_vertices;
			}

			// Phase 2: Compute Tangents and Bitangents
			std::vector<glm::vec3> tangents(outVertices.size(), glm::vec3(0.0f));
			std::vector<glm::vec3> bitangents(outVertices.size(), glm::vec3(0.0f));

			for (size_t i = 0; i < outIndices.size(); i += 3) {
				uint32_t i0 = outIndices[i];
				uint32_t i1 = outIndices[i + 1];
				uint32_t i2 = outIndices[i + 2];

				const glm::vec3& p0 = outVertices[i0].position;
				const glm::vec3& p1 = outVertices[i1].position;
				const glm::vec3& p2 = outVertices[i2].position;

				const glm::vec2& uv0 = outVertices[i0].uv;
				const glm::vec2& uv1 = outVertices[i1].uv;
				const glm::vec2& uv2 = outVertices[i2].uv;

				glm::vec3 edge1 = p1 - p0;
				glm::vec3 edge2 = p2 - p0;
				glm::vec2 deltaUV1 = uv1 - uv0;
				glm::vec2 deltaUV2 = uv2 - uv0;

				float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

				// Prevent division by zero on degenerate UV maps
				if (std::isinf(f) || std::isnan(f)) f = 1.0f;

				glm::vec3 tangent(
					f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
					f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
					f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
				);

				glm::vec3 bitangent(
					f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
					f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
					f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)
				);

				tangents[i0] += tangent;
				tangents[i1] += tangent;
				tangents[i2] += tangent;

				bitangents[i0] += bitangent;
				bitangents[i1] += bitangent;
				bitangents[i2] += bitangent;
			}

			// Phase 3: Orthogonalize Tangents (Gram-Schmidt) and Pack
			for (size_t i = 0; i < outVertices.size(); ++i) {
				const glm::vec3& n = tempNormals[i];
				const glm::vec3& t = tangents[i];
				const glm::vec3& b = bitangents[i];

				// Gram-Schmidt orthogonalize: t = t - (n * dot(n, t))
				glm::vec3 orthoTangent = glm::normalize(t - n * glm::dot(n, t));

				// Calculate bitangent sign mapping (determines handedness of the tangent space)
				float bitangentSign = (glm::dot(glm::cross(n, t), b) < 0.0f) ? -1.0f : 1.0f;

				// Fallback for flat surfaces where tangent might generate zero length
				if (glm::length(orthoTangent) < 0.001f) orthoTangent = glm::vec3(1.0f, 0.0f, 0.0f);

				outVertices[i].tangent = PackTangent(orthoTangent, bitangentSign);
			}

			CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Parsed %u faces -> %llu unique vertices (%llu bytes), %llu indices (%llu bytes)", mesh->face_count, static_cast<unsigned long long>(outVertices.size()), static_cast<unsigned long long>(outVertices.size() * sizeof(StaticVertex)), static_cast<unsigned long long>(outIndices.size()), static_cast<unsigned long long>(outIndices.size() * sizeof(uint32_t)));

			fast_obj_destroy(mesh);
			return true;
		}
		VulkanMeshManager::WorkerPayload::~WorkerPayload() {
			if (m_CompleteVertexAlloc.allocation && m_CompleteVertexAlloc.storage) {
				{
					std::lock_guard lk(*m_CompleteVertexAlloc.storage->m_Mutex);
					m_CompleteVertexAlloc.storage->m_Block.free(m_CompleteVertexAlloc.allocation);
				}
				m_CompleteVertexAlloc.storage->RemoveStrongRef();
				m_CompleteVertexAlloc.storage->RemoveWeakRef();
				m_CompleteVertexAlloc = {};
			}
		}

		VulkanMeshManager::WorkerPayload::WorkerPayload(WorkerPayload&& other) noexcept {
			m_VertexData = std::move(other.m_VertexData);
			m_IndexData = std::move(other.m_IndexData);
			m_CompleteVertexAlloc = other.m_CompleteVertexAlloc;

			m_AABB.bxCenter = other.m_AABB.bxCenter;
			m_AABB.byCenter = other.m_AABB.byCenter;
			m_AABB.bzCenter = other.m_AABB.bzCenter;
			m_AABB.bxExtent = other.m_AABB.bxExtent;
			m_AABB.byExtent = other.m_AABB.byExtent;
			m_AABB.bzExtent = other.m_AABB.bzExtent;

			other.Release();
		}

		VulkanMeshManager::WorkerPayload& VulkanMeshManager::WorkerPayload::operator=(WorkerPayload&& other) noexcept {
			m_VertexData = std::move(other.m_VertexData);
			m_IndexData = std::move(other.m_IndexData);

			if (m_CompleteVertexAlloc.allocation && m_CompleteVertexAlloc.storage) {
				{
					std::lock_guard lk(*m_CompleteVertexAlloc.storage->m_Mutex);
					m_CompleteVertexAlloc.storage->m_Block.free(m_CompleteVertexAlloc.allocation);
				}
				m_CompleteVertexAlloc.storage->RemoveStrongRef();
				m_CompleteVertexAlloc.storage->RemoveWeakRef();
			}

			m_CompleteVertexAlloc = other.m_CompleteVertexAlloc;

			m_AABB.bxCenter = other.m_AABB.bxCenter;
			m_AABB.byCenter = other.m_AABB.byCenter;
			m_AABB.bzCenter = other.m_AABB.bzCenter;
			m_AABB.bxExtent = other.m_AABB.bxExtent;
			m_AABB.byExtent = other.m_AABB.byExtent;
			m_AABB.bzExtent = other.m_AABB.bzExtent;

			other.Release();
			return *this;
		}

		void VulkanMeshManager::WorkerPayload::Release() {
			if (m_CompleteVertexAlloc.storage) {
				//m_CompleteVertexAlloc.storage->RemoveStrongRef();
				m_CompleteVertexAlloc = {};
			}
		}

	}
}