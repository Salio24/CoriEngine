#pragma once
#include <cmath>

#include "Vulkan/VulkanEngine.hpp"
#include "RenderGraph.hpp"
#include "Core/Time.hpp"
#include "Vulkan/VulkanUploadSubsystem.hpp"
#include "Vulkan/VulkanMeshManager.hpp"
#include "Vulkan/VulkanShaderManager.hpp"
#include "Vulkan/VulkanLayoutManager.hpp"
#include "Vulkan/VulkanTextureManager.hpp"
#include "Vulkan/VulkanMaterialSystem.hpp"
#include "FileSystem/PathManager.hpp"
#include "Image.hpp"

namespace Cori {
	namespace Graphics {
		using BatchIndex = uint32_t;
		using DrawGroupIndex = uint32_t;

		class Renderer {
		public:
			class DrawGroup {
			public:
				DrawGroup() = default;
				explicit DrawGroup(const Core::Handle<ShaderEffect> shaderEffect) : m_ShaderEffect(shaderEffect) {}

				[[nodiscard]] uint32_t GetBatchCount() const {
					return m_BatchCount;
				}

				void IncrementBatchCounter() {
					m_BatchCount++;
				}

				void DecrementBatchCounter() {
					m_BatchCount--;
				}

				Core::Handle<ShaderEffect> m_ShaderEffect;
			private:
				uint32_t m_BatchCount{ 0 };
			};

			class Batch {
			public:
				Batch() = default;
				explicit Batch(const Core::Handle<Mesh> mesh) : m_Mesh(mesh) {}

				[[nodiscard]] uint32_t GetObjectCount() const {
					return m_ObjectCount;
				}

				void IncrementObjectCounter() {
					m_ObjectCount++;
				}

				void DecrementObjectCounter() {
					m_ObjectCount--;
				}

				Core::Handle<Mesh> m_Mesh;
			private:
				uint32_t m_ObjectCount{ 0 };
			};

			struct BatchGPUInfo {
				Core::Handle<Mesh> mesh;
				DrawGroupIndex owner{ 0 };
			};

			struct RenderObject {
				RenderObject() = default;
				RenderObject(const glm::mat4& transform, const glm::vec4& uvOffsets, const Core::Handle<Material> material, const BatchIndex batch) : m_Transform(transform), m_UVOffsets(uvOffsets), m_Material(material), m_OwnerBatch(batch) {}
				alignas(16) glm::mat4 m_Transform{ 0.0f };
				alignas(16) glm::vec4 m_UVOffsets{ 0.0f, 0.0f, 1.0f, 1.0f };
				Core::Handle<Material> m_Material;
				BatchIndex m_OwnerBatch{ 0 };
				uint32_t valid{ 0 };
			};

			VulkanFlatSlotMap<RenderObject> m_Objects{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Render Object SlotMap" };

			Core::FlatSlotMap<Batch, 0, false> m_Batches;
			VulkanDynamicVector<BatchGPUInfo> m_BatchGPUInfo{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Batch GPU Data" };

			Core::FlatSlotMap<DrawGroup, 0, false> m_DrawGroups;
			//std::array<VulkanBuffer, FRAMES_IN_FLIGHT> m_RedirectBuffers; //per instance <ObjectHandle> CPU - write, GPU - compute read, semi-retained mode

			uint32_t m_TotalObjectCount{ 0 };

			std::unordered_map<Core::Handle<ShaderEffect>, std::pair<DrawGroupIndex, std::unordered_map<Core::Handle<Mesh>, BatchIndex>>> m_SubBatchLookup;
			//FIXME: need to tell the renderer when a mesh or a shader effect was deleted, to free the batch or draw group

			[[nodiscard]] std::expected<Core::Handle<RenderObject>, ErrorCode> RegisterObject(const Core::Handle<Mesh> mesh, const Core::Handle<Material> material, const glm::mat4& transform, const glm::vec4& UVs = { 0.0f, 0.0f, 1.0f, 1.0f } ) {
				auto shaderEffect = VulkanMaterialSystem::GetMaterialShaderEffect(material);

				if (!shaderEffect) {
					return std::unexpected(shaderEffect.error());
				}

				if (!VulkanMeshManager::IsHandleValid(mesh)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto [groupIndex, batchIndex] = FindAppropriateGroupAndBatch(shaderEffect.value(), mesh);

				m_Batches[batchIndex].IncrementObjectCounter();
				m_TotalObjectCount++;

				return m_Objects.Emplace(transform, UVs, material, batchIndex);
			}

			void UnregisterObject(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Renderer }, "Invalid RenderObject handle was passed to UnregisterObject.");
					return;
				}

				const auto& object = std::as_const(m_Objects)[handle];

				auto& ownerBatch = m_Batches[object.m_OwnerBatch];
				ownerBatch.DecrementObjectCounter();
				m_TotalObjectCount--;

				if (ownerBatch.GetObjectCount() == 0) {
					DestroyBatch(object.m_OwnerBatch);
				}

				m_Objects.Remove(handle);
			}

			[[nodiscard]] bool IsHandleValid(const Core::Handle<RenderObject> handle) const {
				return m_Objects.IsHandleValid(handle);
			}

			[[nodiscard]] std::expected<std::reference_wrapper<const glm::mat4>, ErrorCode> GetRenderObjectTransform(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::cref(std::as_const(m_Objects)[handle].m_Transform);
			}

			std::expected<void, ErrorCode> ChangeRenderObjectTransform(const Core::Handle<RenderObject> handle, const glm::mat4& newTransform) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				m_Objects[handle]->m_Transform = newTransform;

				return {};
			}

			[[nodiscard]] std::expected<std::reference_wrapper<const glm::vec4>, ErrorCode> GetRenderObjectUVOffsets(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::cref(std::as_const(m_Objects)[handle].m_UVOffsets);
			}

			std::expected<void, ErrorCode> ChangeRenderObjectUVOffsets(const Core::Handle<RenderObject> handle, const glm::vec4& newUVOffsets) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				m_Objects[handle]->m_UVOffsets = newUVOffsets;

				return {};
			}

			[[nodiscard]] std::expected<Core::Handle<Mesh>, ErrorCode> GetRenderObjectMesh(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return m_Batches[std::as_const(m_Objects)[handle].m_OwnerBatch].m_Mesh;
			}

			std::expected<void, ErrorCode> ChangeRenderObjectMesh(const Core::Handle<RenderObject> handle, const Core::Handle<Mesh> newMesh) {
				if (!IsHandleValid(handle) || !VulkanMeshManager::IsHandleValid(newMesh)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				auto batchID = std::as_const(m_Objects)[handle].m_OwnerBatch;
				auto& batch = m_Batches[batchID];

				auto oldMesh = batch.m_Mesh;
				if (oldMesh == newMesh) {
					return {};
				}

				batch.DecrementObjectCounter();
				if (batch.GetObjectCount() == 0) {
					DestroyBatch(batchID);
				}

				auto drawGroupID = std::as_const(m_BatchGPUInfo)[batchID].owner;
				auto& group = m_DrawGroups[drawGroupID];

				auto [newGroup, newBatch] = FindAppropriateGroupAndBatch(group.m_ShaderEffect, newMesh);
				m_Objects[handle]->m_OwnerBatch = newBatch;
				m_Batches[newBatch].IncrementObjectCounter();

				return {};
			}

			[[nodiscard]] std::expected<Core::Handle<Material>, ErrorCode> GetRenderObjectMaterial(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::as_const(m_Objects)[handle].m_Material;
			}

			std::expected<void, ErrorCode> ChangeRenderObjectMaterial(const Core::Handle<RenderObject> handle, const Core::Handle<Material> newMaterial) {
				if (!IsHandleValid(handle) || !VulkanMaterialSystem::IsHandleValid(newMaterial)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				const auto& constObjectRef = std::as_const(m_Objects)[handle];

				if (newMaterial == constObjectRef.m_Material) {
					return {};
				}

				auto newShaderEffect = VulkanMaterialSystem::GetMaterialShaderEffect(newMaterial);
				auto oldShaderEffect = VulkanMaterialSystem::GetMaterialShaderEffect(constObjectRef.m_Material);
				if (oldShaderEffect) {
					if (oldShaderEffect.value() == newShaderEffect.value()) {
						m_Objects[handle]->m_Material = newMaterial;
						return {};
					}
				}

				auto& oldBatch = m_Batches[constObjectRef.m_OwnerBatch];
				auto mesh = oldBatch.m_Mesh;

				oldBatch.DecrementObjectCounter();
				if (oldBatch.GetObjectCount() == 0) {
					DestroyBatch(constObjectRef.m_OwnerBatch);
				}

				auto [newGroup, newBatch] = FindAppropriateGroupAndBatch(newShaderEffect.value(), mesh);
				m_Objects[handle]->m_OwnerBatch = newBatch;
				m_Batches[newBatch].IncrementObjectCounter();

				return {};
			}

			static void OnMaterialShaderEffectChanged(const Core::Handle<Material> material, [[maybe_unused]] const Core::Handle<ShaderEffect> oldShaderEffect, const Core::Handle<ShaderEffect> newShaderEffect) {
				for (auto it = Get().m_Objects.cbegin(); it != Get().m_Objects.cend(); ++it) {
					if (it->m_Material == material) {
						auto& oldBatch = Get().m_Batches[it->m_OwnerBatch];
						auto mesh = oldBatch.m_Mesh;

						oldBatch.DecrementObjectCounter();
						if (oldBatch.GetObjectCount() == 0) {
							Get().DestroyBatch(it->m_OwnerBatch);
						}

						auto [newGroup, newBatch] = Get().FindAppropriateGroupAndBatch(newShaderEffect, mesh);
						Get().m_Objects[it.GetHandle()]->m_OwnerBatch = newBatch;
						Get().m_Batches[newBatch].IncrementObjectCounter();
					}
				}
			}

		private:
			[[nodiscard]] std::pair<DrawGroupIndex, BatchIndex> FindAppropriateGroupAndBatch(const Core::Handle<ShaderEffect> shaderEffect, const Core::Handle<Mesh> mesh) {
				auto [it, groupInserted] = m_SubBatchLookup.try_emplace(shaderEffect, std::pair<DrawGroupIndex, std::unordered_map<Core::Handle<Mesh>, BatchIndex>>{});

				if (groupInserted) {
					auto handle = m_DrawGroups.Emplace(shaderEffect);

					it->second.first = handle.GetIndex();
					it->second.second.reserve(64);
				}

				auto [it_, batchInserted] = it->second.second.try_emplace(mesh, BatchIndex{});

				if (batchInserted) {
					auto handle = m_Batches.Emplace(mesh);

					m_BatchGPUInfo.Resize(m_Batches.Capacity());

					m_BatchGPUInfo[handle.GetIndex()] = BatchGPUInfo{ mesh, it->second.first };

					it_->second = handle.GetIndex();

					auto& group = m_DrawGroups[it->second.first];
					group.IncrementBatchCounter();
				}

				return std::make_pair(it->second.first, it_->second);
			}

			void DestroyBatch(const BatchIndex batchID) {
				auto groupID = std::as_const(m_BatchGPUInfo)[batchID].owner;

				auto& group = m_DrawGroups[groupID];
				group.DecrementBatchCounter();

				auto mesh = m_Batches[batchID].m_Mesh;

				if (group.GetBatchCount() == 0) {
					DestroyGroup(groupID);
				} else {
					m_SubBatchLookup.at(group.m_ShaderEffect).second.erase(mesh);
				}

				m_Batches.Remove(m_Batches.GetIndexHandle(batchID));
			}

			void DestroyGroup(const DrawGroupIndex groupID) {
				auto& group = m_DrawGroups[groupID];

				m_SubBatchLookup.erase(group.m_ShaderEffect);

				m_DrawGroups.Remove(m_DrawGroups.GetIndexHandle(groupID));
			}

		public:
			Renderer() {
				m_Objects.Reserve(256);
				m_Batches.Reserve(128);
				m_DrawGroups.Reserve(16);
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

				std::vector<StaticVertex> vertices = {
					// Bottom-left
					{ glm::vec3(-0.5f, -0.5f, depth), 0, 0, glm::vec2(0,0), 0xFFFFFFFF },

					// Bottom-right
					{ glm::vec3( 0.5f, -0.5f, depth), 0, 0, glm::vec2(1,0), 0xFFFFFFFF },

					// Top-right
					{ glm::vec3( 0.5f,  0.5f, depth), 0, 0, glm::vec2(1,1), 0xFFFFFFFF },

					// Top-left
					{ glm::vec3(-0.5f,  0.5f, depth), 0, 0, glm::vec2(0,1), 0xFFFFFFFF }
				};

				std::vector<uint32_t> indices = {
					0, 1, 2,   // first triangle
					2, 3, 0    // second triangle
				};

				quad = VulkanMeshManager::CreateMesh();
				VulkanMeshManager::LoadToMesh(quad, vertices, std::move(indices));

				auto image = Image::Create(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "placeholders/uv_sample.png");

				texture = VulkanTextureManager::CreateTexture(vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, { image->GetHeight(), image->GetWidth(), 1 }, 1, 1, vk::SampleCountFlagBits::e1, "UV sample texture");
				VulkanTextureManager::UpdateTexture(texture, std::span{ static_cast<Byte*>(image->GetPixelData()), image->GetHeight() * image->GetWidth() * 4 }, { 0, 0, 0 }, { image->GetHeight(), image->GetWidth(), 1 }, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 });
				VulkanTextureManager::ChangeView(texture, vk::ImageViewType::e2D, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

				auto shaderEffect = VulkanMaterialSystem::CreateShaderEffect(testShader, {}, {}, "Test Shader Effect");

				MaterialData materialData {
					.colorFactor = { 1.0f, 1.0f, 1.0f, 1.0f },
					.albedoTexture = texture,
					.albedoSampler = 0
				};

				MaterialData materialData_ {
					.colorFactor = { 1.0f, 1.0f, 1.0f, 1.0f },
					.albedoTexture = VulkanTextureManager::GetPlaceholderTexture(),
					.albedoSampler = 0
				};

				material2 = VulkanMaterialSystem::CreateMaterial(shaderEffect, materialData_, "Test Material 2");

				material = VulkanMaterialSystem::CreateMaterial(shaderEffect, materialData, "Test Material");

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5, 1.0f));

				RegisterObject(quad, material, transform);
				transform = glm::translate(transform, glm::vec3(+1.0f, 0.0f, 0.0f));

				RegisterObject(quad, material2, transform);

				VulkanMaterialSystem::AddOnShaderEffectSwappedListener(OnMaterialShaderEffectChanged);
			}

			~Renderer() {}

			static void Init();

			static void Shutdown();

			static Renderer& Get();

			void Render() {
				CORI_PROFILE_FUNCTION();

				VulkanEngine::Get().CPUFrameStart();

				auto commandOffsetsBuffer = VulkanVirtualBufferAllocator::CreateVirtualUploadBuffer(m_DrawGroups.RawSize() * sizeof(uint32_t), 4, VulkanEngine::GetNextFrameInFlight(), "Draw Group Command Offsets");

				m_GraphPassRegistry.Reset();
				m_GraphResourceRegistry.Reset(VulkanEngine::GetFrameIndex());
				uint32_t frameInFlightIndex = VulkanEngine::GetCurrentFrameInFlight();
				RenderGraph graph(m_GraphPassRegistry, m_GraphResourceRegistry);

				auto ObjectBufferHandle = graph.ImportFlatSlotMap(m_Objects, "Object Data");
				auto BatchInfoBufferHandle = graph.ImportDynamicVector(m_BatchGPUInfo, "Batch Info");

				auto GroupCommandOffsetsBufferHandle = graph.ImportBuffer(commandOffsetsBuffer, "Group Command Offsets Buffer"); // per group <uint32_t>, CPU - write, GPU - compute read, semi-retained mode

				uint32_t maxObjectCount = m_Objects.RawSize();
				uint32_t maxBatchCount = m_Batches.RawSize();
				uint32_t maxGroupCount = m_DrawGroups.RawSize();

				struct ComputePS{
					uint64_t objectDataBuffer;
					uint64_t compactedObjectIDBuffer;

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

				struct DrawPS{
					uint64_t objectDataBuffer;
					uint64_t compactedObjectIDBuffer;
					uint64_t materialDataBuffer;
					uint64_t shaderEffectDataBuffer;
					uint64_t meshDataBuffer;
					uint64_t textureAssetTable;
					uint64_t batchInfo;
				};

				auto DrawCommandBufferHandle = graph.CreateBuffer({ INDIRECT_COMMAND_SIZE * m_Batches.RawSize(), INDIRECT_COMMAND_SIZE }, "Draw Command Buffer"); //per batch <VkDrawIndexedIndirectCommand> CPU - none, GPU - compute write -> command submit read, immediate mode
				auto DrawCommandCountBufferHandle = graph.CreateBuffer({ m_DrawGroups.RawSize() * sizeof(uint32_t), alignof(uint32_t) }, "Draw Command Count"); //per group <uint32_t> CPU - none, GPU - compute write -> indirect command read, immediate mode
				auto BatchIntermediateInfoBufferHandle = graph.CreateBuffer({ m_Batches.RawSize() * sizeof(uint64_t), alignof(uint64_t) }, "Batch Intermediate Info"); //per batch <uint64_t> CPU - none, GPU - compute write/read atomic, immediate mode
				auto InstanceAtomicCounterHandle = graph.CreateBuffer({ sizeof(uint32_t), alignof(uint32_t) }, "Instance Atomic Counter"); //atomic uint32_t, CPU - none, GPU - compute write/read atomic, immediate mode
				auto CompactedInstanceListBufferHandle = graph.CreateBuffer({ m_Objects.RawSize() * sizeof(uint32_t), alignof(uint32_t) }, "Compacted Instance List Buffer"); //per visible instance <uint32_t> CPU - none, GPU - compute write -> vertex/fragment read, immediate mode

				auto& bufferCleanupPass = graph.CreatePass("Buffer Cleanup");
				bufferCleanupPass.Writes(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
				bufferCleanupPass.Writes(InstanceAtomicCounterHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
				bufferCleanupPass.Writes(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
				bufferCleanupPass.AssignWork([=](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					auto& dccb = registry.GetResource(DrawCommandCountBufferHandle);
					auto& iac = registry.GetResource(InstanceAtomicCounterHandle);
					auto& cilb = registry.GetResource(CompactedInstanceListBufferHandle);
					commandBuffer.fillBuffer(dccb.GetHeapHandle(), dccb.GetStartOffset(), dccb.GetSize(), 0);
					commandBuffer.fillBuffer(iac.GetHeapHandle(), iac.GetStartOffset(), iac.GetSize(), 0);
					commandBuffer.fillBuffer(cilb.GetHeapHandle(), cilb.GetStartOffset(), cilb.GetSize(), 0);
				});

				auto& cullPass = graph.CreatePass("Cull Pass");
				cullPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cullPass.Reads(ObjectBufferHandle);
				cullPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					ComputePS ps {
						.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
						.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
						.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
						.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
						.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
						.batchInfos = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA(),
						.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
						.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
						.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
						.totalInstanceCount = maxObjectCount,
						.totalBatchCount = maxBatchCount
					};

					commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

					auto result = VulkanShaderManager::GetShader(cullShader);
					CORI_CORE_ASSERT(result, "Failed to get cullShader. Error: {}", to_string(result.error()));
					result.value().get().Bind(commandBuffer);
					commandBuffer.dispatch(std::ceil(maxObjectCount / 64.0f), 1, 1);
				});

				auto& cmgPass = graph.CreatePass("Indirect Command Generation Pass");
				cmgPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Writes(DrawCommandBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Writes(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Writes(InstanceAtomicCounterHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Reads(BatchInfoBufferHandle);
				cmgPass.Reads(GroupCommandOffsetsBufferHandle);
				cmgPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					ComputePS ps {
						.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
						.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
						.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
						.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
						.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
						.batchInfos = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA(),
						.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
						.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
						.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
						.totalInstanceCount = maxObjectCount,
						.totalBatchCount = maxBatchCount
					};

					commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

					auto result = VulkanShaderManager::GetShader(cmgShader);
					CORI_CORE_ASSERT(result, "Failed to get cmgShader. Error: {}", to_string(result.error()));
					result.value().get().Bind(commandBuffer);
					commandBuffer.dispatch(std::ceil(maxBatchCount / 64.0f), 1, 1);
				});

				auto& compactPass = graph.CreatePass("Compact Pass");
				compactPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				compactPass.Writes(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				compactPass.Reads(ObjectBufferHandle);
				compactPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					ComputePS ps {
						.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
						.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
						.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
						.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
						.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
						.batchInfos = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA(),
						.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
						.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
						.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
						.totalInstanceCount = maxObjectCount,
						.totalBatchCount = maxBatchCount
					};

					commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

					auto result = VulkanShaderManager::GetShader(compactShader);
					CORI_CORE_ASSERT(result, "Failed to get compactShader. Error: {}", to_string(result.error()));
					result.value().get().Bind(commandBuffer);
					commandBuffer.dispatch(std::ceil(maxObjectCount / 64.0f), 1, 1);
				});

				auto& indirectPass = graph.CreatePass("Indirect Pass");
				indirectPass.Reads(ObjectBufferHandle);
				indirectPass.Reads(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead });
				indirectPass.Reads(DrawCommandBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
				indirectPass.Reads(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
				indirectPass.Reads(BatchInfoBufferHandle);
				indirectPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					DrawPS ps {
						.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
						.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
						.materialDataBuffer = VulkanMaterialSystem::GetMaterialSlotMapBDA(),
						.shaderEffectDataBuffer = VulkanMaterialSystem::GetShaderEffectDataBufferBDA(),
						.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
						.textureAssetTable = VulkanTextureManager::GetTextureAssetTableBDA(),
						.batchInfo = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA()
					};

					commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(DrawPS), &ps);

					VulkanGlobalLayoutManager::BindDescriptorBuffer(commandBuffer);
					commandBuffer.bindIndexBuffer(VulkanMeshManager::GetIndexBuffer().m_Buffer, 0, vk::IndexType::eUint32);

					PipelineState currentPipelineState;
					currentPipelineState.Change(commandBuffer);

					uint32_t currentCommandOffset = 0;
					Core::Handle<ShaderEffect> currentShaderEffect;

					auto& indirectCommandBuffer = registry.GetResource(DrawCommandBufferHandle);
					auto& indirectCommandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle);

					//temp
					vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
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

					for (uint32_t i = 0; i < m_DrawGroups.RawSize(); i++) {
						if (m_DrawGroups.IsIndexValid(i)) {
							auto& group = m_DrawGroups[i];
							if (group.GetBatchCount() != 0) {
								if (group.m_ShaderEffect != currentShaderEffect) {
									auto pairHandleResult = VulkanMaterialSystem::GetShaderEffectShaderPair(group.m_ShaderEffect);

									CORI_CORE_ASSERT(pairHandleResult, "Group hold an invalid  shader effect handle.");

									auto shaderResult = VulkanShaderManager::GetShader(pairHandleResult.value());

									CORI_CORE_ASSERT(shaderResult, "Group hold a shader effect that point to invalid Vert+Frag Shader pair.");

									shaderResult.value().get().Bind(commandBuffer);

									auto pipelineState = VulkanMaterialSystem::GetShaderEffectPipelineState(group.m_ShaderEffect).value();

									if (pipelineState.get() != currentPipelineState) {
										pipelineState.get().Change(commandBuffer);
									}

									currentPipelineState = pipelineState;
									currentShaderEffect = group.m_ShaderEffect;
								}

								commandBuffer.drawIndexedIndirectCount(indirectCommandBuffer.GetHeapHandle(), currentCommandOffset * INDIRECT_COMMAND_SIZE + indirectCommandBuffer.GetStartOffset(), indirectCommandCountBuffer.GetHeapHandle(), i * sizeof(uint32_t) + indirectCommandCountBuffer.GetStartOffset(), group.GetBatchCount(), INDIRECT_COMMAND_SIZE);
								currentCommandOffset += group.GetBatchCount();
							}
						}
					}

					commandBuffer.endRendering();
				});

				graph.Compile(VulkanEngine::GetFrameIndex(), VulkanEngine::GetNextFrameInFlight());
				auto& frameData = VulkanEngine::Get().GPUFrameBegin();

				for (uint32_t i = 0; i < m_DrawGroups.RawSize(); ++i) {
					uint32_t value = 0;
					if (m_DrawGroups.IsIndexValid(i)) {
						value = m_DrawGroups[i].GetBatchCount();
					}

					commandOffsetsBuffer.UploadToAllocation<uint32_t>(std::span{ &value, 1 }, sizeof(uint32_t) * i);
				}

				m_Objects.Sync();
				m_BatchGPUInfo.Sync();
				VulkanEngine::Get().GPUFrameMiddlePointSync();
				if (!frameData.m_SkippedFrame) {
					graph.Execute(frameData.m_CommandBuffer);
				}

				VulkanEngine::Get().GPUFrameEnd();
			}
		private:
			static constexpr uint32_t INDIRECT_COMMAND_SIZE = 20;

			Core::Handle<ComputeShader> cullShader;
			Core::Handle<ComputeShader> cmgShader;
			Core::Handle<ComputeShader> compactShader;

			Core::Handle<VertFragShaderPair> testShader;
			Core::Handle<VertFragShaderPair> defaultShader;

			Core::Handle<Mesh> quad;
			Core::Handle<Material> material;
			Core::Handle<Material> material2;

			Core::Handle<Texture2> texture;
			RenderGraphResourceRegistry m_GraphResourceRegistry;
			RenderGraphPassRegistry m_GraphPassRegistry;

			static std::unique_ptr<Renderer> s_Instance;

		};
	}
}