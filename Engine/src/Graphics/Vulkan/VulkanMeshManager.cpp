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
			RenderThreadCommandQueue::Push([handle]() mutable {
				if (!IsHandleValid(handle)) {
					return;
				}

				if (Get().m_Meshes.IsIndexOccupied(handle.GetIndex())) {
					return;
				}

				uint32_t index = handle.GetIndex();
				uint32_t size = Get().m_MeshMetadata.size();
				if (index >= size) {
					Get().m_MeshMetadata.resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
				}

				Get().m_Meshes.EmplaceAt(handle.GetIndex());
				Get().AssignPlaceholder(handle);
			});
		}

		void VulkanMeshManager::Load(const Core::Handle<Mesh> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name) {
			RegisterAtSlot(handle);
			Core::Application::SubmitWorkerTask([path = std::move(path), name = std::move(name), handle, id, gen, vectorKey]() mutable {
				auto FinalizeLoad = [](const Core::Handle<Mesh> handle_, const uint32_t gen_, const uint32_t vectorKey_, std::expected<WorkerPayload, ErrorCode>&& payload) {
					if (payload) {
						RenderThreadCommandQueue::Push([handle_, gen_, payload = std::move(payload.value())]() mutable {
							if (!IsHandleValid(handle_)) {
								return;
							}

							if (Get().m_HandleAllocator.GetGeneration(handle_) != gen_) {
								return;
							}

							if (!Get().m_Meshes.IsIndexOccupied(handle_.GetIndex())) {
								uint32_t index = handle_.GetIndex();
								uint32_t size = Get().m_MeshMetadata.size();
								if (index >= size) {
									Get().m_MeshMetadata.resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
								}

								Get().m_Meshes.EmplaceAt(handle_.GetIndex());
							}

							Get().DestroyMesh(handle_);
							auto& meta = Get().m_MeshMetadata[handle_.GetIndex()];
							meta.placeholderAssigned = false;

							Get().LoadToMesh(handle_, std::move(std::get<std::vector<StaticVertex>>(payload.m_VertexData)), std::move(payload.m_IndexData), gen_, payload.m_CompleteVertexAlloc);

							payload.Release();
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

							if (!Get().m_Meshes.IsIndexOccupied(handle_.GetIndex())) {
								uint32_t index = handle_.GetIndex();
								uint32_t size = Get().m_MeshMetadata.size();
								if (index >= size) {
									Get().m_MeshMetadata.resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
								}

								Get().m_Meshes.EmplaceAt(handle_.GetIndex());
								Get().AssignPlaceholder(handle_);
							}

							SetAssetStatus(handle_, AssetStatus::eLoadFailed);
						});
					}
				};

				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, path.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Load({}), mesh handle [{},{}], failed to open asset file '{}', error '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead.", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::enum_to_string(readError));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eFailedToOpenFile));
					return;
				}

				JsonAssetDataCombined data;
				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(data, buffer);
				if (parseError) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Load({}), mesh handle [{},{}], failed to parse asset file '{}', asset will not be loaded, and a placeholder will be assigned to the handle instead. Error: {}", id, handle.GetIndex(), handle.GetVersion(), path.string(), glz::format_error(parseError, buffer));
					FinalizeLoad(handle, gen, vectorKey, std::unexpected(ErrorCode::eParseFailure));
					return;
				}

				std::filesystem::path objPath = path.replace_filename(data.AssetData.obj);
				std::vector<StaticVertex> vertexData;
				std::vector<uint32_t> indexData;
				LoadObjToEngine(objPath.string().c_str(), vertexData, indexData);

				CompleteVertexAllocation ca;

				//this will need to change to support multiple vertex layouts
				vma::VirtualAllocationCreateInfo verticesAllocInfo {
					.size = vertexData.size() * sizeof(StaticVertex),
					.alignment = alignof(StaticVertex),
					.flags = s_VertexAllocFlags
				};

				for (auto& storage : Get().m_VertexStorages) {
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
					VertexStorage* vertexStorage = Get().AllocateNewVertexStorage();
					auto discoverResult = vertexStorage->DiscoverBoth();
					CORI_CORE_ASSERT(discoverResult, "Discovery failed for newly created vertex block for some reason.");

					std::lock_guard lk(*vertexStorage->m_Mutex);
					auto [result2, alloc] = vertexStorage->m_Block.virtualAllocate(verticesAllocInfo, ca.offset);
					CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new vertices in a newly created vertex storage, error: {}", vk::to_string(result2));
					ca.allocation = alloc;
					ca.storage = vertexStorage;
				}

				FinalizeLoad(handle, gen, vectorKey, WorkerPayload(std::move(vertexData), std::move(indexData), ca));
			});
		}
	}
}