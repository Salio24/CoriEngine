#pragma once
#include "Vulkan/VulkanEngine.hpp"
#include "Vulkan/VulkanResourceTracker.hpp"
#include "RenderGraph.hpp"
#include "Core/Time.hpp"
#include "Vulkan/VulkanUploadManager.hpp"
#include "Vulkan/VulkanMeshManager.hpp"
#include "Vulkan/VulkanShaderManager.hpp"
#include "Vulkan/VulkanLayoutManager.hpp"
#include "Vulkan/VulkanTextureManager.hpp"
#include "Vulkan/VulkanMaterialSystem.hpp"
#include "FileSystem/PathManager.hpp"
#include "Image.hpp"

//FIXME: need explicit Renderer lifetime control, its deleted after VulkanEngine has been shutdown

namespace Cori {
	namespace Graphics {
		const ResourceState StateA = { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal };
		const ResourceState StateB = { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral };
		const ResourceState StateC = { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eColorAttachmentOptimal };
		const ResourceState StateD = { vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::ImageLayout::eDepthAttachmentOptimal };

		using ObjectHandle = uint32_t;
		using SubBatchHandle = uint32_t;
		using DrawGroupHandle = uint32_t;

		class Renderer {
		public:
			struct DrawGroup {
				DrawGroup() {
					batches.reserve(4);
				}

				[[nodiscard]] uint32_t GetBatchCount() const {
					return batches.size();
				}

				void AddBatch(const SubBatchHandle handle) {
					batches.emplace_back(handle);
				}

				void RemoveBatch(const SubBatchHandle handle) {
					auto& batch = Get().m_SubBatches[handle];
					if (batches.size() < batch.indexInGroup + 1) {
						return;
					}

					if (batches.size() == batch.indexInGroup + 1) {
						batches.pop_back();
					} else {
						SubBatchHandle backHandle = batches.back();
						batches[batch.indexInGroup] = backHandle;
						Get().m_SubBatches[backHandle].indexInGroup = batch.indexInGroup;
						batches.pop_back();
					}
				}

				ShaderEffectHandle effect{ 0 };
			private:
				std::vector<SubBatchHandle> batches;
			};

			struct SubBatch {
				SubBatch() {
					objects.reserve(64);
				}

				[[nodiscard]] uint32_t AddObject(const ObjectHandle handle) {
					uint32_t index = objects.size();
					objects.emplace_back(handle);
					return index;
				}

				void RemoveObject(const uint32_t index) {
					if (objects.size() < index + 1) {
						return;
					}

					if (objects.size() == index + 1) {
						objects.pop_back();
					} else {
						ObjectHandle backHandle = objects.back();
						objects[index] = backHandle;
						Get().m_ObjectMetadata[backHandle].indexInSubBatch = index;
						objects.pop_back();
					}
				}

				MeshHandle mesh{ 0 };

				uint32_t indexInGroup{ 0 };

			private:
				std::vector<ObjectHandle> objects;
			};

			struct BatchInfo {
				//uint32_t firstSortedInstanceIndex;
				//uint32_t totalInstanceCount;
				MeshHandle mesh{ 0 };
				DrawGroupHandle owner{ 0 };
			};

			struct ObjectData {
				alignas(16) glm::mat4 transform{ 0.0f };
				alignas(16) glm::vec4 uvOffsets{ 0.0f, 0.0f, 1.0f, 1.0f };
				MaterialHandle material{ 0 };
				SubBatchHandle subBatch{ 0 };
				bool valid{ false };
				bool pad1{ false };
				bool pad2{ false };
				bool pad3{ false };
				uint32_t pad4{ 0 };
			};

			struct ObjectMetadata {
				uint32_t indexInSubBatch;
				DrawGroupHandle drawGroup;
			};

			struct ObjectUpdateRequest {
				glm::mat4 transform{ 0.0f };
				glm::vec4 uvOffsets{ 1000.0f };
				MaterialHandle material{ UINT32_MAX };
				MeshHandle mesh{ UINT32_MAX };
			};

			std::vector<DrawGroup> m_Groups;
			std::vector<DrawGroupHandle> m_GroupHoles;
			DrawGroupHandle m_NextGroupHandle{ 0 };
			static constexpr DrawGroupHandle MAX_GROUPS = 128;

			std::vector<SubBatch> m_SubBatches;
			std::vector<SubBatchHandle> m_SubBatchHoles;
			SubBatchHandle m_NextSubBatchHandle{ 0 };
			static constexpr SubBatchHandle MAX_SUBBATCHES = 512;

			std::vector<ObjectData> m_ObjectData;
			std::vector<ObjectMetadata> m_ObjectMetadata;
			std::vector<ObjectHandle> m_ObjectHoles;
			ObjectHandle m_NextObjectHandle{ 0 };
			static constexpr ObjectHandle MAX_OBJECTS = 1024 * 1024 * 1; // 1-mil TODO: there should be no limit, need resizing

			AmazingBufferHandle m_ObjectBufferHandle; //per instance <ObjectData> CPU - write, GPU - compute/vertex/fragment read - retained mode
			AmazingBufferHandle m_BatchInfoBufferHandle; //per batch <BatchInfo> CPU - write, GPU - compute read - retained mode

			//std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_RedirectBuffers; //per instance <ObjectHandle> CPU - write, GPU - compute read, semi-retained mode
			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_GroupCommandOffsets; // per group <uint32_t>, CPU - write, GPU - compute read, semi-retained mode
			std::array<bool, FRAMES_IN_FLIGHT> m_SemiRetainedDataDirty{ false };

			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_DrawCommandBuffers; //per batch <VkDrawIndexedIndirectCommand> CPU - none, GPU - compute write -> command submit read, immediate mode
			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_DrawCommandCountBuffers; //per group <uint32_t> CPU - none, GPU - compute write -> indirect command read, immediate mode

			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_BatchIntermediateInfoBuffers; //per batch <uint64_t> CPU - none, GPU - compute write/read atomic, immediate mode
			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_AtomicInstance; //atomic uint32_t, CPU - none, GPU - compute write/read atomic, immediate mode

			std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_CompactedInstanceListBuffers; //per visible instance <uint32_t> CPU - none, GPU - compute write -> vertex/fragment read, immediate mode

			uint32_t m_TotalObjectCount{ 0 };

			std::unordered_map<ShaderEffectHandle, std::pair<DrawGroupHandle, std::unordered_map<MeshHandle, SubBatchHandle>>> m_SubBatchLookup;

			static ObjectHandle RegisterObject(const MeshHandle mesh, const MaterialHandle material, const glm::mat4& transform, const glm::vec4& UVs = { 0.0f, 0.0f, 1.0f, 1.0f } ) {
				ObjectHandle freeHandle;
				if (!Get().m_ObjectHoles.empty()) {
					freeHandle = Get().m_GroupHoles.back();
					Get().m_ObjectHoles.pop_back();
				} else {
					freeHandle = Get().m_NextObjectHandle++;
					CORI_CORE_ASSERT(freeHandle < MAX_GROUPS - 1, "Renderer out of object slots.");
				}

				auto& objectData = Get().m_ObjectData[freeHandle];
				auto& objectMetadata = Get().m_ObjectMetadata[freeHandle];

				objectData.transform = transform;
				objectData.uvOffsets = UVs;

				auto [groupHandle, batchHandle] = FindAppropriateGroupAndBatch(VulkanMaterialSystem::GetMaterialShaderEffect(material), mesh);

				objectData.material = material;
				objectData.subBatch = batchHandle;
				objectData.valid = true;

				auto& batch = Get().m_SubBatches[batchHandle];
				objectMetadata.drawGroup = groupHandle;
				objectMetadata.indexInSubBatch = batch.AddObject(freeHandle);

				Get().SyncObjectGPUData(freeHandle);

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i ++) {
					Get().m_SemiRetainedDataDirty[i] = true;
				}

				Get().m_TotalObjectCount++;
				return freeHandle;
			}

			void UpdateBuffers() {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				if (m_SemiRetainedDataDirty[frameIndex]) {
					m_SemiRetainedDataDirty[frameIndex] = false;

					std::vector<Byte> groupCommandOffsetPayload(sizeof(uint32_t) * m_Groups.size());

					for (uint32_t i = 0; i < m_Groups.size() - 1; i++) {
						uint32_t batchCount = m_Groups[i].GetBatchCount();
						memcpy(groupCommandOffsetPayload.data() + (i + 1) * sizeof(uint32_t), &batchCount, sizeof(uint32_t));
					}

					VulkanUploadManager::UploadPart part {
						.resource = m_GroupCommandOffsets[frameIndex],
						.range = VulkanUploadManager::BufferUploadRange{ 0, 4 },
						.data = std::move(groupCommandOffsetPayload)
					};

					VulkanUploadManager::UploadRequest uploadRequest {
						.uploadParts = std::move(part),
						.uploadType = VulkanUploadManager::UploadType::FrameCritical
					};

					VulkanUploadManager::SubmitUploadRequest(std::move(uploadRequest));
				}
			}

			static void UnregisterObject(const ObjectHandle handle) {
				if (handle >= MAX_OBJECTS) {
					return;
				}

				auto& objectData = Get().m_ObjectData[handle];
				if (!objectData.valid) {
					return;
				}

				objectData.valid = false;

				auto& objectMetadata = Get().m_ObjectMetadata[handle];

				Get().m_SubBatches[objectData.subBatch].RemoveObject(objectMetadata.indexInSubBatch);

				Get().m_ObjectHoles.emplace_back(handle);
				Get().SyncObjectGPUData(handle);

				Get().m_TotalObjectCount--;
			}

			static void UpdateObjectData(const ObjectHandle handle, const ObjectUpdateRequest& update) {
				if (handle >= MAX_OBJECTS) {
					return;
				}

				auto& objectData = Get().m_ObjectData[handle];
				if (!objectData.valid) {
					return;
				}

				bool updated = false;

				if (update.transform != glm::mat4(0.0f)) {
					objectData.transform = update.transform;
					updated = true;
				}

				if (update.uvOffsets != glm::vec4(1000.0f)) {
					objectData.uvOffsets = update.uvOffsets;
					updated = true;
				}

				if (update.material != UINT32_MAX || update.mesh != UINT32_MAX) {
					auto& objectMetadata = Get().m_ObjectMetadata[handle];
					auto& currentBatch = Get().m_SubBatches[objectData.subBatch];

					ShaderEffectHandle oldShaderEffectHandle = Get().m_Groups[objectMetadata.drawGroup].effect;
					MeshHandle oldMeshHandle = currentBatch.mesh;

					ShaderEffectHandle newShaderEffectHandle;
					MeshHandle newMeshHandle = update.mesh != UINT32_MAX ? newMeshHandle = update.mesh : newMeshHandle = oldMeshHandle;

					if (update.material != UINT32_MAX) {
						if (update.material != objectData.material) {
							newShaderEffectHandle = VulkanMaterialSystem::GetMaterialShaderEffect(update.material);
							objectData.material = update.material;
							updated = true;
						} else {
							newShaderEffectHandle = oldShaderEffectHandle;
						}
					} else {
						newShaderEffectHandle = oldShaderEffectHandle;
					}

					if (oldShaderEffectHandle != newShaderEffectHandle || oldMeshHandle != newMeshHandle) {
						auto [groupHandle, batchHandle] = FindAppropriateGroupAndBatch(newShaderEffectHandle, newMeshHandle);

						currentBatch.RemoveObject(handle);
						auto& newBatch = Get().m_SubBatches[batchHandle];
						objectData.subBatch = batchHandle;
						objectMetadata.indexInSubBatch = newBatch.AddObject(handle);
						objectMetadata.drawGroup = groupHandle;
						updated = true;
					}
				}

				if (updated) {
					Get().SyncObjectGPUData(handle);
				}
			}

		private:
			void SyncObjectGPUData(const ObjectHandle handle) {
				AmazingBuffer::UpdateData patch {
					.offset = handle * sizeof(ObjectData),
					.alignment = alignof(ObjectData),
					.data = std::vector<Byte>(sizeof(ObjectData))
				};

				memcpy(patch.data.data(), &m_ObjectData[handle], sizeof(ObjectData));

				auto& objectDataBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_ObjectBufferHandle);
				objectDataBuffer.SubmitUpdate(std::move(patch));
			}

			[[nodiscard]] static std::pair<DrawGroupHandle, SubBatchHandle> FindAppropriateGroupAndBatch(const ShaderEffectHandle shaderEffectHandle, const MeshHandle meshHandle) {
				auto [it, groupInserted] = Get().m_SubBatchLookup.try_emplace(shaderEffectHandle, std::pair<DrawGroupHandle, std::unordered_map<MeshHandle, SubBatchHandle>>{});

				if (groupInserted) {
					DrawGroupHandle freeHandle;
					if (!Get().m_GroupHoles.empty()) {
						freeHandle = Get().m_GroupHoles.back();
						Get().m_GroupHoles.pop_back();
					} else {
						freeHandle = Get().m_NextGroupHandle++;
						CORI_CORE_ASSERT(freeHandle < MAX_GROUPS - 1, "Renderer out of draw group slots.");
					}

					Get().m_Groups[freeHandle].effect = shaderEffectHandle;

					it->second.first = freeHandle;
				}

				auto [it_, batchInserted] = it->second.second.try_emplace(meshHandle, SubBatchHandle{});

				if (batchInserted) {
					SubBatchHandle freeHandle;
					if (!Get().m_SubBatchHoles.empty()) {
						freeHandle = Get().m_SubBatchHoles.back();
						Get().m_SubBatchHoles.pop_back();
					} else {
						freeHandle = Get().m_NextSubBatchHandle++;
						CORI_CORE_ASSERT(freeHandle < MAX_SUBBATCHES - 1, "Renderer out of draw sub batch slots.");
					}

					Get().m_SubBatches[freeHandle].mesh = meshHandle;
					Get().m_Groups[it->second.first].AddBatch(freeHandle);

					it_->second = freeHandle;

					BatchInfo info {
						.mesh = meshHandle,
						.owner = it->second.first
					};

					AmazingBuffer::UpdateData patch {
						.offset = freeHandle * sizeof(BatchInfo),
						.alignment = alignof(BatchInfo),
						.data = std::vector<Byte>(sizeof(BatchInfo))
					};

					memcpy(patch.data.data(), &info, sizeof(BatchInfo));

					auto& batchInfoBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_BatchInfoBufferHandle);
					batchInfoBuffer.SubmitUpdate(std::move(patch));
				}

				return std::make_pair(it->second.first, it_->second);
			}
		public:

			static void Defragment() {

			}


			Renderer() {
				m_Groups.reserve(64);
				m_SubBatches.reserve(128);
				std::ifstream file(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/CullingShader.spv", std::ios::ate | std::ios::binary);
				if (!file.is_open()) {
					throw std::runtime_error("failed to open file!");
				}

				std::vector<Byte> buffer(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
				file.close();

				cullShader = VulkanShaderManager::CreateComputeShader(buffer.data(), buffer.size(), "cullMain", "Cull Compute Shader");
				cmgShader = VulkanShaderManager::CreateComputeShader(buffer.data(), buffer.size(), "cmgMain", "Indirect Command Generation Compute Shader");
				compactShader = VulkanShaderManager::CreateComputeShader(buffer.data(), buffer.size(), "compactMain", "Instance Compacting Compute Shader");


				std::ifstream file_(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/TestShader.spv", std::ios::ate | std::ios::binary);
				if (!file_.is_open()) {
					throw std::runtime_error("failed to open file!");
				}

				std::vector<Byte> buffer_(file_.tellg());
				file_.seekg(0, std::ios::beg);
				file_.read(reinterpret_cast<char*>(buffer_.data()), static_cast<std::streamsize>(buffer_.size()));
				file_.close();

				testShader = VulkanShaderManager::CreateVertexShaderPair(buffer_.data(), buffer_.size(), "vertMain", "fragMain", "Test Shader");

				std::ifstream file__(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/DefaultShader.spv", std::ios::ate | std::ios::binary);
				if (!file__.is_open()) {
					throw std::runtime_error("failed to open file!");
				}

				std::vector<Byte> buffer__(file__.tellg());
				file__.seekg(0, std::ios::beg);
				file__.read(reinterpret_cast<char*>(buffer__.data()), static_cast<std::streamsize>(buffer__.size()));
				file__.close();

				defaultShader = VulkanShaderManager::CreateVertexShaderPair(buffer__.data(), buffer__.size(), "vertMain", "fragMain", "Default Shader");


				float depth = 0.0f;

				std::vector<Vertex> vertices = {
					// Bottom-left
					{ glm::vec3(-0.5f, -0.5f, depth),   0.0f, glm::vec3(0,0,1),   0.0f, glm::vec4(1,1,1,1) },

					// Bottom-right
					{ glm::vec3( 0.5f, -0.5f, depth),   1.0f, glm::vec3(0,0,1),   0.0f, glm::vec4(1,1,1,1) },

					// Top-right
					{ glm::vec3( 0.5f,  0.5f, depth),   1.0f, glm::vec3(0,0,1),   1.0f, glm::vec4(1,1,1,1) },

					// Top-left
					{ glm::vec3(-0.5f,  0.5f, depth),   0.0f, glm::vec3(0,0,1),   1.0f, glm::vec4(1,1,1,1) }
				};

				std::vector<uint32_t> indices = {
					0, 1, 2,   // first triangle
					2, 3, 0    // second triangle
				};

				quad = VulkanMeshManager::CreateMesh(vertices.data(), vertices.size() * sizeof(Vertex), indices.data(), indices.size() * sizeof(uint32_t));

				auto image = Image::Create(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "placeholders/uv_sample.png");

				texture = VulkanTextureManager::CreateTextureTest(image->GetPixelData(), image->GetHeight() * image->GetWidth() * 4, vk::Format::eR8G8B8A8Srgb, { image->GetHeight(), image->GetWidth() }, "Test Texture");
				#if 1
				auto shaderEffect = VulkanMaterialSystem::CreateShaderEffect(testShader, {}, {}, "Test Shader Effect");

				MaterialData materialData {
					.colorFactor = { 1.0f, 1.0f, 1.0f, 1.0f },
					.albedoTexture = texture,
					.albedoSampler = 0
				};

				material = VulkanMaterialSystem::CreateMaterial(shaderEffect, materialData, "Test Material");

				AmazingBuffer::CreateInfo objectBufferInfo {
					.size = MAX_OBJECTS * sizeof(ObjectData),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.queueFamilyIndices = { VulkanEngine::GetGraphicsQueueFamilyIndex() },
					.name = "Renderer Object Data buffer"
				};

				AmazingBuffer::CreateInfo batchInfoBufferInfo {
					.size = MAX_SUBBATCHES * sizeof(BatchInfo),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.queueFamilyIndices = { VulkanEngine::GetGraphicsQueueFamilyIndex() },
					.name = "Renderer Batch Info buffer"
				};

				m_ObjectBufferHandle = VulkanUploadManager::CreateAmazingBuffer(objectBufferInfo);
				m_BatchInfoBufferHandle = VulkanUploadManager::CreateAmazingBuffer(batchInfoBufferInfo);

				uint32_t transferQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex();

				std::vector<uint32_t> queueFamilyIndices{ VulkanEngine::GetGraphicsQueueFamilyIndex() };

				bool transferQueueInVector = false;
				for (auto familyIndex : queueFamilyIndices) {
					if (familyIndex == transferQueueFamilyIndex) {
						transferQueueInVector = true;
					}
				}

				vk::SharingMode sharingMode;
				std::vector<uint32_t> queueFamilyIndices_;

				if (transferQueueInVector && queueFamilyIndices.size() == 1) {
					sharingMode = vk::SharingMode::eExclusive;
				} else if (transferQueueInVector && queueFamilyIndices.size() != 1) {
					sharingMode = vk::SharingMode::eConcurrent;
					queueFamilyIndices_ = queueFamilyIndices;
				} else {
					queueFamilyIndices.emplace_back(transferQueueFamilyIndex);
					sharingMode = vk::SharingMode::eConcurrent;
					queueFamilyIndices_ = queueFamilyIndices;
				}

				vma::AllocationCreateInfo allocInfo {
					.usage = vma::MemoryUsage::eAuto
				};

				vk::BufferCreateInfo redirectBufferInfo {
					.size = MAX_OBJECTS * sizeof(ObjectHandle),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
					.sharingMode = sharingMode,
					.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices_.size()),
					.pQueueFamilyIndices = queueFamilyIndices_.data(),
				};

				vk::BufferCreateInfo groupCommandOffsetBufferInfo {
					.size = MAX_GROUPS * sizeof(uint32_t),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
					.sharingMode = sharingMode,
					.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices_.size()),
					.pQueueFamilyIndices = queueFamilyIndices_.data(),
				};

				vk::BufferCreateInfo drawCommandBufferInfo{
					.size = MAX_SUBBATCHES * INDIRECT_COMMAND_SIZE,
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eIndirectBuffer,
					.sharingMode = vk::SharingMode::eExclusive
				};

				vk::BufferCreateInfo drawCommandCountBufferInfo{
					.size = MAX_GROUPS * sizeof(uint32_t),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
					.sharingMode = vk::SharingMode::eExclusive
				};

				vk::BufferCreateInfo batchIntermediateInfoBufferInfo {
					.size = MAX_SUBBATCHES * sizeof(uint64_t),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.sharingMode = vk::SharingMode::eExclusive
				};

				vk::BufferCreateInfo atomicBufferInfo {
					.size = sizeof(uint32_t),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
					.sharingMode = vk::SharingMode::eExclusive
				};

				vk::BufferCreateInfo compactedInstanceListBufferInfo {
					.size = MAX_OBJECTS * sizeof(uint32_t),
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
					.sharingMode = vk::SharingMode::eExclusive
				};

				for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
					//std::string redirectName = std::format("Instance Redirect Buffer {}", i);
					//m_RedirectBuffers[i] = VulkanBuffer::Create({ .allocationCreateInfo = &allocInfo, .bufferCreateInfo = &redirectBufferInfo, .name = redirectName.c_str() });

					std::string groupCommandOffsetName = std::format("Group Command Offset Buffer {}", i);
					m_GroupCommandOffsets[i] = VulkanBuffer::Create({ .bufferCreateInfo = &groupCommandOffsetBufferInfo, .allocationCreateInfo = &allocInfo, .name = groupCommandOffsetName.c_str() });

					std::string drawCommandName = std::format("Draw Command Buffer {}", i);
					m_DrawCommandBuffers[i] = VulkanBuffer::Create({ .bufferCreateInfo = &drawCommandBufferInfo, .allocationCreateInfo = &allocInfo, .name = drawCommandName.c_str() });

					std::string drawCommandCountName = std::format("Draw Command Count Buffer {}", i);
					m_DrawCommandCountBuffers[i] = VulkanBuffer::Create({ .bufferCreateInfo = &drawCommandCountBufferInfo, .allocationCreateInfo = &allocInfo, .name = drawCommandCountName.c_str() });

					std::string batchIntermediateInfoName = std::format("Batch Intermediate Info Buffer {}", i);
					m_BatchIntermediateInfoBuffers[i] = VulkanBuffer::Create({ .bufferCreateInfo = &batchIntermediateInfoBufferInfo, .allocationCreateInfo = &allocInfo, .name = batchIntermediateInfoName.c_str() });

					std::string atomicInstanceName = std::format("Atomic Instance Count Buffer {}", i);
					m_AtomicInstance[i] = VulkanBuffer::Create({ .bufferCreateInfo = &atomicBufferInfo, .allocationCreateInfo = &allocInfo, .name = atomicInstanceName.c_str() });

					std::string compactedInstanceListName = std::format("Compacted Instance List Buffer {}", i);
					m_CompactedInstanceListBuffers[i] = VulkanBuffer::Create({ .bufferCreateInfo = &compactedInstanceListBufferInfo, .allocationCreateInfo = &allocInfo, .name = compactedInstanceListName.c_str() });
				}

				m_Groups.resize(MAX_GROUPS);
				m_SubBatches.resize(MAX_SUBBATCHES);
				m_ObjectData.resize(MAX_OBJECTS);
				m_ObjectMetadata.resize(MAX_OBJECTS);
				#endif
			}

			~Renderer() {
				buffer.Destroy();
				image.Destroy();
			}

			static Renderer& Get() {
				static Renderer instance;
				return instance;
			}

			struct ComputePS {
				uint64_t objectDataBuffer;
				uint64_t compactedObjectIDBuffer;
				//RedirectID* redirectBuffer;

				uint64_t bii;

				uint64_t commandBuffer;
				uint64_t commandCountBuffer;
				uint64_t batchInfos;
				uint64_t meshDataBuffer;
				uint64_t globalAtomic;
				uint64_t commandOffsets;

				uint32_t totalInstanceCount;
				uint32_t totalBatchCount;
			};

			struct DrawPS {
				uint64_t objectDataBuffer;
				uint64_t compactedObjectIDBuffer;
				uint64_t vertexBuffer;
				uint64_t materialDataBuffer;
				uint64_t shaderEffectDataBuffer;
			};

			static void Render() {
				Get();
				CORI_PROFILE_FUNCTION();

				static bool oneshot = false;
				if (!oneshot) {
					oneshot = true;

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5, 1.0f));

					RegisterObject(Get().quad, Get().material, transform, { 0.125f, 0.125f, 0.25f, 0.25f });
				}

				Get().UpdateBuffers();
				Get().m_GraphPassRegistry.Reset();
				Get().m_GraphResourceRegistry.Reset();
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				RenderGraph graph(Get().m_GraphPassRegistry, Get().m_GraphResourceRegistry);

				auto ObjectBufferHandle = graph.ImportBuffer(VulkanUploadManager::GetAmazingBuffer(Get().m_ObjectBufferHandle).GetCurrentFrameLocalBuffer(), "Object Data Buffer");
				auto BatchInfoBufferHandle = graph.ImportBuffer(VulkanUploadManager::GetAmazingBuffer(Get().m_BatchInfoBufferHandle).GetCurrentFrameLocalBuffer(), "Batch Info Buffer");

				auto GroupCommandOffsetsBufferHandle = graph.ImportBuffer(Get().m_GroupCommandOffsets[frameIndex], "Group Command Offsets Buffer");

				auto DrawCommandBufferHandle = graph.ImportBuffer(Get().m_DrawCommandBuffers[frameIndex], "Draw Command Buffer");
				auto DrawCommandCountBufferHandle = graph.ImportBuffer(Get().m_DrawCommandCountBuffers[frameIndex], "Draw Command Count Buffer");

				auto BatchIntermediateInfoBufferHandle = graph.ImportBuffer(Get().m_BatchIntermediateInfoBuffers[frameIndex], "Batch Intermediate Info Buffer");
				auto InstanceAtomicCounterHandle = graph.ImportBuffer(Get().m_AtomicInstance[frameIndex], "Instance Atomic Counter Buffer");
				auto CompactedInstanceListBufferHandle = graph.ImportBuffer(Get().m_CompactedInstanceListBuffers[frameIndex], "Compacted Instance List Buffer Buffer");

				auto MeshAssetBufferHandle = graph.ImportBuffer(VulkanMeshManager::GetFrameLocalMeshAssetBuffer(), "Mesh Asset Buffer");

				auto VertexBufferHandle = graph.ImportBuffer(VulkanMeshManager::GetVertexSSBO(), "Vertex Buffer");

				auto MaterialDataBuffer = graph.ImportBuffer(VulkanMaterialSystem::GetFrameLocalMaterialDataBuffer(), "Material Data Buffer");

				auto ShaderEffectDataBufferHandle = graph.ImportBuffer(VulkanMaterialSystem::GetFrameLocalShaderEffectDataBuffer(), "Material Data Buffer");

				uint32_t objectCount = Get().m_NextObjectHandle;
				uint32_t batchCount = Get().m_NextSubBatchHandle;
				uint32_t groupCount = Get().m_NextGroupHandle;

				//uint32_t objectCount = Get().m_ObjectData.size();
				//uint32_t batchCount = Get().m_SubBatches.size();
				//uint32_t groupCount = Get().m_Groups.size();


				ComputePS cps {
					.objectDataBuffer = Get().m_GraphResourceRegistry.GetBuffer(ObjectBufferHandle).GetBDA(),
					.compactedObjectIDBuffer = Get().m_GraphResourceRegistry.GetBuffer(CompactedInstanceListBufferHandle).GetBDA(),
					.bii = Get().m_GraphResourceRegistry.GetBuffer(BatchIntermediateInfoBufferHandle).GetBDA(),
					.commandBuffer = Get().m_GraphResourceRegistry.GetBuffer(DrawCommandBufferHandle).GetBDA(),
					.commandCountBuffer = Get().m_GraphResourceRegistry.GetBuffer(DrawCommandCountBufferHandle).GetBDA(),
					.batchInfos = Get().m_GraphResourceRegistry.GetBuffer(BatchInfoBufferHandle).GetBDA(),
					.meshDataBuffer = Get().m_GraphResourceRegistry.GetBuffer(MeshAssetBufferHandle).GetBDA(),
					.globalAtomic = Get().m_GraphResourceRegistry.GetBuffer(InstanceAtomicCounterHandle).GetBDA(),
					.commandOffsets = Get().m_GraphResourceRegistry.GetBuffer(GroupCommandOffsetsBufferHandle).GetBDA(),
					.totalInstanceCount = objectCount,
					.totalBatchCount = batchCount
				};

				DrawPS dps {
					.objectDataBuffer = Get().m_GraphResourceRegistry.GetBuffer(ObjectBufferHandle).GetBDA(),
					.compactedObjectIDBuffer = Get().m_GraphResourceRegistry.GetBuffer(CompactedInstanceListBufferHandle).GetBDA(),
					.vertexBuffer = Get().m_GraphResourceRegistry.GetBuffer(VertexBufferHandle).GetBDA(),
					.materialDataBuffer = Get().m_GraphResourceRegistry.GetBuffer(MaterialDataBuffer).GetBDA(),
					.shaderEffectDataBuffer = Get().m_GraphResourceRegistry.GetBuffer(ShaderEffectDataBufferHandle).GetBDA()
				};

				auto& bufferCleanupPass = graph.CreatePass("Buffer Cleanup");
				bufferCleanupPass.Writes(DrawCommandCountBufferHandle, { {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite}, BufferRange{ 0, VK_WHOLE_SIZE } });
				bufferCleanupPass.Writes(InstanceAtomicCounterHandle, { {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite}, BufferRange{ 0, VK_WHOLE_SIZE } });
				bufferCleanupPass.Writes(CompactedInstanceListBufferHandle, { {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite}, BufferRange{ 0, VK_WHOLE_SIZE } });
				bufferCleanupPass.AssignWork([&](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					commandBuffer.fillBuffer(registry.GetBuffer(DrawCommandCountBufferHandle).m_Buffer, 0, VK_WHOLE_SIZE, 0);
					commandBuffer.fillBuffer(registry.GetBuffer(InstanceAtomicCounterHandle).m_Buffer, 0, VK_WHOLE_SIZE, 0);
					commandBuffer.fillBuffer(registry.GetBuffer(CompactedInstanceListBufferHandle).m_Buffer, 0, VK_WHOLE_SIZE, 0);
				});

				auto& cullPass = graph.CreatePass("Cull Pass");
				cullPass.Writes(BatchIntermediateInfoBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite }, BufferRange{ 0, sizeof(uint64_t) * batchCount } });
				cullPass.Reads(ObjectBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(ObjectData) * objectCount } });
				cullPass.AddPushConstants(vk::ShaderStageFlagBits::eAll, &cps, sizeof(cps));
				cullPass.AssignWork([&](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					VulkanShaderManager::GetShaderObject(Get().cullShader).Bind(commandBuffer);
					commandBuffer.dispatch(ceil(objectCount / 64.0f), 1, 1);
				});

				auto& cmgPass = graph.CreatePass("Indirect Command Generation Pass");
				cmgPass.Writes(BatchIntermediateInfoBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite }, BufferRange{ 0, sizeof(uint64_t) * batchCount } });
				cmgPass.Writes(DrawCommandBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite }, BufferRange{ 0, groupCount * INDIRECT_COMMAND_SIZE } });
				cmgPass.Writes(DrawCommandCountBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite }, BufferRange{ 0, sizeof(uint32_t) * groupCount } });
				cmgPass.Writes(InstanceAtomicCounterHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite }, BufferRange{ 0, VK_WHOLE_SIZE } });
				cmgPass.Reads(BatchInfoBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(BatchInfo) * batchCount } });
				cmgPass.Reads(MeshAssetBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(MeshAssetInfo) * VulkanMeshManager::GetLoadedMeshCount() } });
				cmgPass.Reads(GroupCommandOffsetsBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(uint32_t) * groupCount } });
				cmgPass.AddPushConstants(vk::ShaderStageFlagBits::eAll, &cps, sizeof(cps));
				cmgPass.AssignWork([&](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					VulkanShaderManager::GetShaderObject(Get().cmgShader).Bind(commandBuffer);
					commandBuffer.dispatch(ceil(batchCount / 64.0f), 1, 1);
				});

				auto& compactPass = graph.CreatePass("Compact Pass");
				compactPass.Writes(BatchIntermediateInfoBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite }, BufferRange{ 0, sizeof(uint64_t) * batchCount } });
				compactPass.Writes(CompactedInstanceListBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite }, BufferRange{ 0, sizeof(uint32_t) * objectCount } });
				compactPass.Reads(ObjectBufferHandle, { { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(ObjectData) * objectCount } });
				compactPass.AddPushConstants(vk::ShaderStageFlagBits::eAll, &cps, sizeof(cps));
				compactPass.AssignWork([&](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					VulkanShaderManager::GetShaderObject(Get().compactShader).Bind(commandBuffer);
					commandBuffer.dispatch(ceil(objectCount / 64.0f), 1, 1);
				});

				auto& indirectPass = graph.CreatePass("Indirect Pass");
				indirectPass.Reads(ObjectBufferHandle, { { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(ObjectData) * objectCount } });
				indirectPass.Reads(CompactedInstanceListBufferHandle, { { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(uint32_t) * objectCount } });
				indirectPass.Reads(VertexBufferHandle, { { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(uint32_t) * objectCount } });
				indirectPass.Reads(MaterialDataBuffer, { { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(MaterialCombinedData) * VulkanMaterialSystem::GetLoadedMaterialCount() } });
				indirectPass.Reads(ShaderEffectDataBufferHandle, { { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead }, BufferRange{ 0, sizeof(ShaderEffectData) * VulkanMaterialSystem::GetLoadedShaderEffectCount() } });
				indirectPass.Reads(DrawCommandBufferHandle, { { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead }, BufferRange{ 0, VK_WHOLE_SIZE } });
				indirectPass.Reads(DrawCommandCountBufferHandle, { { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead }, BufferRange{ 0, VK_WHOLE_SIZE } });
				indirectPass.AddPushConstants(vk::ShaderStageFlagBits::eAll, &dps, sizeof(dps));
				indirectPass.AssignWork([&](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					VulkanGlobalLayoutManager::BindDescriptorBuffer(commandBuffer);
					commandBuffer.bindIndexBuffer(VulkanMeshManager::GetIndexBuffer().m_Buffer, 0, vk::IndexType::eUint32);

					PipelineState curentPipelineState;
					curentPipelineState.Change(commandBuffer);

					uint32_t currentCommandOffset = 0;
					ShaderEffectHandle curentShaderEffect = UINT32_MAX;

					auto& indirectCommandBuffer = registry.GetBuffer(DrawCommandBufferHandle);
					auto& indirectCommandCountBuffer = registry.GetBuffer(DrawCommandCountBufferHandle);

					//FIXME: use the swapchain data feature of the rendergraph, and dont render to the swapchain framebuffer directly, but to an HDR texure, but this is fine for testing.
					//temp
					vk::ClearValue clearColor = vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f);
					vk::RenderingAttachmentInfo attachmentInfo = {
						.imageView = VulkanEngine::GetSwapChainImageView(),
						.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
						.loadOp = vk::AttachmentLoadOp::eClear,
						.storeOp = vk::AttachmentStoreOp::eStore,
						.clearValue = clearColor
					};

					vk::RenderingInfo renderingInfo = {
						.renderArea = {.offset = {0, 0}, .extent = VulkanEngine::GetSwapChainExtent()},
						.layerCount = 1,
						.colorAttachmentCount = 1,
						.pColorAttachments = &attachmentInfo
					};
					commandBuffer.beginRendering(renderingInfo);
					//temp


					for (uint32_t i = 0; i < Get().m_Groups.size(); i++) {
						auto& group = Get().m_Groups[i];
						if (group.GetBatchCount() != 0) {
							if (group.effect != curentShaderEffect) {
								VulkanMaterialSystem::BindShaderEffect(group.effect, curentPipelineState, commandBuffer);
							}

							commandBuffer.drawIndexedIndirectCount(indirectCommandBuffer.m_Buffer, currentCommandOffset * INDIRECT_COMMAND_SIZE, indirectCommandCountBuffer.m_Buffer, i * sizeof(uint32_t), group.GetBatchCount(), INDIRECT_COMMAND_SIZE);
							currentCommandOffset += group.GetBatchCount();
						}
					}

					commandBuffer.endRendering();
				});

				graph.Compile();
				auto& frameData = VulkanEngine::Get().BeginFrame();
				if (!frameData.m_SkippedFrame) {
					graph.Execute(frameData.m_CommandBuffer);
				}

				if (!frameData.m_SkippedFrame && false) {
					vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f);
					vk::RenderingAttachmentInfo attachmentInfo = {
						.imageView = VulkanEngine::GetSwapChainImageView(),
						.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
						.loadOp = vk::AttachmentLoadOp::eClear,
						.storeOp = vk::AttachmentStoreOp::eStore,
						.clearValue = clearColor
					};

					vk::RenderingInfo renderingInfo = {
						.renderArea = {.offset = {0, 0}, .extent = VulkanEngine::GetSwapChainExtent()},
						.layerCount = 1,
						.colorAttachmentCount = 1,
						.pColorAttachments = &attachmentInfo
					};

					frameData.m_CommandBuffer.beginRendering(renderingInfo);

					VulkanGlobalLayoutManager::BindDescriptorBuffer(frameData.m_CommandBuffer);
					VulkanShaderManager::GetShaderObject(Get().defaultShader).Bind(frameData.m_CommandBuffer);

					frameData.m_CommandBuffer.bindIndexBuffer(VulkanMeshManager::GetIndexBuffer().m_Buffer, 0, vk::IndexType::eUint32);

					struct PushConstants {
						uint64_t vertexBufferAddress{ 0 };
						uint64_t meshAssetDataAddress{ 0 };
					} pc;

					pc.vertexBufferAddress = VulkanMeshManager::GetVertexSSBO().GetBDA();
					pc.meshAssetDataAddress = VulkanMeshManager::GetFrameLocalMeshAssetBuffer().GetBDA();

					frameData.m_CommandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(pc), &pc);

					frameData.m_CommandBuffer.drawIndexed(6, 1, 0, 0, 0);

					frameData.m_CommandBuffer.endRendering();
				}

				VulkanEngine::Get().EndFrame();

				//VulkanResourceTracker* tracker = &VulkanResourceTracker::Get();

				//VulkanUploadManager::Get();

				#if 0
				VulkanUploadManager* manager = &VulkanUploadManager::Get();

				auto res1 = manager->Allocate(1024, 4);
				auto res2 = manager->Allocate(1024, 4);
				manager->SetFence(nullptr);
				manager->test = true;
				auto res3 = manager->Allocate(1024, 4);
				manager->test = false;

				auto res4 = manager->Allocate(512, 4);

				manager->SetFence(nullptr);


				auto res5 = manager->Allocate(1024, 4);


				auto res6 = manager->Allocate(1024, 4);

				auto res7 = manager->Allocate(1024, 4);

				manager->test = true;

				auto res8 = manager->Allocate(1024, 4);

				manager->test = false;

				auto res9 = manager->Allocate(1024, 4);
				auto res10 = manager->Allocate(1024, 4);

				manager->SetFence(nullptr);

				manager->test = true;

				//?????
				auto res11 = manager->Allocate(1024, 4);

				manager->test = false;

				#endif

				#if 0

				ResourceState stateA{
					.stageMask = vk::PipelineStageFlagBits2::eVertexInput,
					.accessMask = vk::AccessFlagBits2::eShaderRead,
				};

				auto opt = VulkanResourceTracker::TransitionBuffer(Get().buffer, 0, VK_WHOLE_SIZE, stateA);

				std::vector<vk::BufferMemoryBarrier2> wholebar = *opt.value();

				ResourceState stateB{
					.stageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.accessMask = vk::AccessFlagBits2::eShaderWrite,
				};

				auto opt1 = VulkanResourceTracker::TransitionBuffer(Get().buffer, 1024, 1024, stateB);

				std::vector<vk::BufferMemoryBarrier2> quaterbar = *opt1.value();

				ResourceState octa{
					.stageMask = vk::PipelineStageFlagBits2::eFragmentShader,
					.accessMask = vk::AccessFlagBits2::eShaderSampledRead,
				};

				auto opt2 = VulkanResourceTracker::TransitionBuffer(Get().buffer, 2048, 1024, stateB);

				std::vector<vk::BufferMemoryBarrier2> octabar = *opt2.value();

				ResourceState half{
					.stageMask = vk::PipelineStageFlagBits2::eTransfer,
					.accessMask = vk::AccessFlagBits2::eTransferWrite,
				};

				auto opt3 = VulkanResourceTracker::TransitionBuffer(Get().buffer, 0, VK_WHOLE_SIZE, half);

				std::vector<vk::BufferMemoryBarrier2> halfbar = *opt3.value();

				//VulkanResourceTracker::UnregisterBuffer(Get().buffer);
				//VulkanResourceTracker::RegisterBuffer(Get().buffer);
				#else

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 0, 10, 0, 1 }, StateA);

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 3, 4, 0, 1 }, StateB);

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 2, 7, 0, 1 }, StateC);

				//RunImageTest(Get().image, { vk::ImageAspectFlagBits::eDepth, 3, 2, 0, 1 }, StateB);

				//VulkanResourceTracker::UnregisterImage(Get().image);
				//VulkanResourceTracker::RegisterImage(Get().image);

				#endif
			}
		private:

			static constexpr uint32_t INDIRECT_COMMAND_SIZE = 32;

			ShaderObjectHandle cullShader;
			ShaderObjectHandle cmgShader;
			ShaderObjectHandle compactShader;

			ShaderObjectHandle testShader;
			ShaderObjectHandle defaultShader;

			MeshHandle quad;
			MaterialHandle material;

			VulkanBuffer buffer;
			VulkanImage image;
			TextureHandle texture;
			RenderGraphResourceRegistry m_GraphResourceRegistry;
			RenderGraphPassRegistry m_GraphPassRegistry;
		};
	}
}