#pragma once
#include "Graphics/Vulkan/VulkanEngine.hpp"
#include "Graphics/Vulkan/VulkanImage.hpp"
#include "Graphics/Vulkan/VulkanBuffer.hpp"
#include "Graphics/Vulkan/VulkanLayoutManager.hpp"
#include "Graphics/Vulkan/VulkanUploadSubsystem.hpp"
#include "Graphics/Vulkan/DeviceLossDebug/VulkanDeviceLossDebug.hpp"
#include "Utility/StringHash.hpp"
#include "Utility/HashCombine.hpp"

#ifndef CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE
	#define CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE 16
#endif

#ifndef CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE
	#define CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE 16
#endif

#ifndef CORI_RENDER_GRAPH_RESOURCE_REGISTRY_INLINE_SIZE
	#define CORI_RENDER_GRAPH_RESOURCE_REGISTRY_INLINE_SIZE 32
#endif

#ifndef CORI_RENDER_GRAPH_PER_PASS_UNIQUE_RESOURCES_SET_INLINE_SIZE
	#define CORI_RENDER_GRAPH_PER_PASS_UNIQUE_RESOURCES_SET_INLINE_SIZE 16
#endif

#ifndef CORI_STALE_RESOURCE_LIFETIME
	#define CORI_STALE_RESOURCE_LIFETIME 128
#endif

namespace Cori {
	namespace Graphics {
		struct ImageCreateInfo {
			vk::ImageCreateInfo imageCreateInfo{};
			vma::AllocationCreateInfo allocationCreateInfo{};
			const char* name = "";
		};

		struct ScratchBufferCreateInfo {
			uint64_t size{ 0 };
			uint64_t alignment{ 4 };
			const char* name{ "" };
		};

		class RenderGraphResourceRegistry;
		class RenderGraphPassRegistry;
		class RenderGraph;

		namespace Internal {
			using PassHandle = uint32_t;

			enum class ResourceOrigin {
				Imported,
				Created
			};

			struct ResourceState {
				vk::PipelineStageFlags2 stageMask{ vk::PipelineStageFlagBits2::eNone };
				vk::AccessFlags2 accessMask{ vk::AccessFlagBits2::eNone };
				vk::ImageLayout imageLayout{ vk::ImageLayout::eUndefined };

				bool operator==(const ResourceState& other) const = default;
			};

			struct ResourceUsage {
				ResourceState desiredState;

				vk::ImageSubresourceRange subrange{};
			};

			struct PooledImageDescription {
				explicit PooledImageDescription(const ImageCreateInfo& fullInfo) {
					flags = fullInfo.imageCreateInfo.flags;
					imageType = fullInfo.imageCreateInfo.imageType;
					format = fullInfo.imageCreateInfo.format;
					extent = fullInfo.imageCreateInfo.extent;
					mipLevels = fullInfo.imageCreateInfo.mipLevels;
					arrayLayers = fullInfo.imageCreateInfo.arrayLayers;
					samples = fullInfo.imageCreateInfo.samples;
					tiling = fullInfo.imageCreateInfo.tiling;
					usage = fullInfo.imageCreateInfo.usage;
				}

				vk::ImageCreateFlags flags;
				vk::ImageType imageType;
				vk::Format format;
				vk::Extent3D extent;
				uint32_t mipLevels;
				uint32_t arrayLayers;
				vk::SampleCountFlagBits samples;
				vk::ImageTiling tiling;
				vk::ImageUsageFlags usage;

				bool operator==(const PooledImageDescription& other) const = default;

				auto operator<=>(const PooledImageDescription& other) const = default;

				struct Hasher {
					std::size_t operator()(const PooledImageDescription& desc) const noexcept {
						uint64_t hash = 0;

						Utility::HashCombine(hash, static_cast<uint32_t>(desc.flags));
						Utility::HashCombine(hash, static_cast<uint32_t>(desc.imageType));
						Utility::HashCombine(hash, static_cast<uint32_t>(desc.format));
						Utility::HashCombine(hash, desc.extent.width);
						Utility::HashCombine(hash, desc.extent.height);
						Utility::HashCombine(hash, desc.extent.depth);
						Utility::HashCombine(hash, desc.mipLevels);
						Utility::HashCombine(hash, desc.arrayLayers);
						Utility::HashCombine(hash, static_cast<uint32_t>(desc.samples));
						Utility::HashCombine(hash, static_cast<uint32_t>(desc.tiling));
						Utility::HashCombine(hash, static_cast<uint32_t>(desc.usage));

						return hash;
					}
				};
			};

			struct PooledImage {
				VulkanImage image;
				uint32_t lastFrameUsed{ 0 };
			};

			struct ImageState {
				bool isUniform{ true };

				ResourceState globalState;

				std::vector<ResourceState> subresourceStates;

				void Reset(const ResourceState& initial) {
					isUniform = true;
					globalState = initial;
					subresourceStates.clear();
				}
			};

			using BufferState = ResourceState;

			using ImageStateHandle = uint32_t;

			class GraphResourceHandleBase {
			protected:
				GraphResourceHandleBase() = default;
				GraphResourceHandleBase(const uint32_t value) : resourceID(value) {}

				operator uint32_t() const { return resourceID; }

				uint32_t resourceID{ UINT32_MAX };

				friend RenderGraphResourceRegistry;
				friend RenderGraphPassRegistry;
				friend RenderGraph;
			};

			struct PassResourceDependency {
				GraphResourceHandleBase resource;
				ResourceUsage usage;
			};

			struct ImageResourceData {
				ImageCreateInfo createInfo;
				VulkanImage image;
				ImageStateHandle stateHandle;
			};

			struct VirtualBufferResourceData {
				ScratchBufferCreateInfo createInfo;
				VulkanVirtualBuffer virtualBuffer;
				BufferState state;
			};

			using DynamicContainerResourceData = const void*;

			struct ResourceNode {
				const char* name{ "" };
				ResourceOrigin origin;
				PassHandle lastProducer{ UINT32_MAX };

				std::variant<ImageResourceData, VirtualBufferResourceData, DynamicContainerResourceData> resourceData;
			};
		}

		template<typename T>
		class GraphResourceHandle : public Internal::GraphResourceHandleBase {};

		struct BufferUsage {
			vk::PipelineStageFlags2 stageMask{ vk::PipelineStageFlagBits2::eNone };
			vk::AccessFlags2 accessMask{ vk::AccessFlagBits2::eNone };

			explicit operator Internal::ResourceUsage() const {
				return { { stageMask, accessMask } };
			}
		};

		struct ImageUsage {
			vk::PipelineStageFlags2 stageMask{ vk::PipelineStageFlagBits2::eNone };
			vk::AccessFlags2 accessMask{ vk::AccessFlagBits2::eNone };
			vk::ImageLayout imageLayout{ vk::ImageLayout::eUndefined };
			vk::ImageSubresourceRange subrange{};

			explicit operator Internal::ResourceUsage() const {
				return { { stageMask, accessMask, imageLayout }, subrange };
			}
		};

		struct SwapChainImageData {
			vk::Image swapChainImage = nullptr;
			vk::ImageView swapChainImageView = nullptr;
			vk::Extent2D imageExtent;
		};

		struct ScratchBufferTag {};
		struct UploadBufferTag {};

		class RenderGraphResourceRegistry {
		public:
			RenderGraphResourceRegistry() {
				m_Nodes.resize(CORI_RENDER_GRAPH_RESOURCE_REGISTRY_INLINE_SIZE);
				m_ImageStateCache.resize(64); //TODO: move to define.
			}

			~RenderGraphResourceRegistry() {
				for (auto& vector : m_ImagePool | std::ranges::views::values) {
					for (auto& pooledImage : vector) {
						if (pooledImage.image.m_Image) {
							DeletionQueue::PushImage(pooledImage.image);
						}
					}
				}
			}

			void Reset(const uint64_t currentFrameIndex) {
				m_SwapChainImageData = {};
				m_SwapChainImageRetrieved = false;
				m_Nodes.clear();
				m_ImageStateCacheSize = 0;

				ClearStalePooledImages(currentFrameIndex);
			}

			VulkanVirtualBuffer& GetResource(const GraphResourceHandle<ScratchBufferTag> handle) {
				auto& node = GetNode(handle);
				auto& resourceData = std::get<Internal::VirtualBufferResourceData>(node.resourceData);
				return resourceData.virtualBuffer;
			}

			VulkanVirtualBuffer& GetResource(const GraphResourceHandle<UploadBufferTag> handle) {
				auto& node = GetNode(handle);
				auto& resourceData = std::get<Internal::VirtualBufferResourceData>(node.resourceData);
				return resourceData.virtualBuffer;
			}

			VulkanImage& GetResource(const GraphResourceHandle<VulkanImage> handle) {
				auto& node = GetNode(handle);
				auto& resourceData = std::get<Internal::ImageResourceData>(node.resourceData);
				return resourceData.image;
			}

			template<typename T>
			const VulkanDynamicVector<T>& GetResource(const GraphResourceHandle<VulkanDynamicVector<T>> handle) {
				auto& node = GetNode(handle);
				auto resourceData = std::get<Internal::DynamicContainerResourceData>(node.resourceData);
				return *static_cast<const VulkanDynamicVector<T>*>(resourceData);
			}

			template<typename T>
			const VulkanFlatSlotMap<T>& GetResource(const GraphResourceHandle<VulkanFlatSlotMap<T>> handle) {
				auto& node = GetNode(handle);
				auto resourceData = std::get<Internal::DynamicContainerResourceData>(node.resourceData);
				return *static_cast<const VulkanFlatSlotMap<T>*>(resourceData);
			}

			void RegisterSwapChainImage(const SwapChainImageData& data) {
				m_SwapChainImageData = data;
			}

			SwapChainImageData& GetSwapChainImageData() {
				m_SwapChainImageRetrieved = true;
				return m_SwapChainImageData;
			}

		protected:
			friend RenderGraph;

			Internal::ResourceNode& GetNode(const Internal::GraphResourceHandleBase handle) {
				return m_Nodes[handle];
			}

			Internal::GraphResourceHandleBase AddNode(const Internal::ResourceNode& node) {
				Internal::GraphResourceHandleBase handle = m_Nodes.size();
				m_Nodes.emplace_back(node);
				return handle;
			}

			Internal::ImageState& GetImageState(const Internal::ImageStateHandle handle) {
				return m_ImageStateCache[handle];
			}

			Internal::ImageStateHandle GetFreeImageState() {
				if (m_ImageStateCacheSize >= m_ImageStateCache.size()) {
					m_ImageStateCache.resize(std::max<std::size_t>(m_ImageStateCache.size() * 1.5f, m_ImageStateCacheSize + 1));
				}

				Internal::ImageState& state = m_ImageStateCache[m_ImageStateCacheSize];
				state.Reset({});

				return m_ImageStateCacheSize++;
			}

			VulkanImage& GetPooledImage(const ImageCreateInfo& info, const uint64_t currentFrameIndex) {
				Internal::PooledImageDescription desc(info);

				auto& bucket = m_ImagePool[desc];

				for (auto& entry : bucket) {
					if (currentFrameIndex - entry.lastFrameUsed >= FRAMES_IN_FLIGHT) {
						entry.lastFrameUsed = currentFrameIndex;
						return entry.image;
					}
				}

				VulkanImage::CreateInfo createInfo {
					.imageCreateInfo = &info.imageCreateInfo,
					.allocationCreateInfo = &info.allocationCreateInfo,
					.name = info.name,
				};

				auto newImage = VulkanImage::Create(createInfo);

				bucket.emplace_back(newImage, currentFrameIndex);

				return bucket.back().image;
			}

			void ClearStalePooledImages(const uint64_t currentFrameIndex) {
				for (auto it = m_ImagePool.begin(); it != m_ImagePool.end(); ) {
					auto& bucket = it->second;

					for (int32_t i = bucket.size() - 1; i >= 0; i--) {
						if (currentFrameIndex - bucket[i].lastFrameUsed > CORI_STALE_RESOURCE_LIFETIME) {
							bucket[i].image.Destroy();

							if (i != bucket.size() - 1) {
								bucket[i] = bucket.back();
							}

							bucket.pop_back();
						}
					}

					if (bucket.empty()) {
						it = m_ImagePool.erase(it);
					} else {
						++it;
					}
				}
			}

		private:

			std::vector<Internal::ResourceNode> m_Nodes;
			std::vector<Internal::ImageState> m_ImageStateCache;

			std::unordered_map<Internal::PooledImageDescription, std::vector<Internal::PooledImage>, Internal::PooledImageDescription::Hasher> m_ImagePool;

			SwapChainImageData m_SwapChainImageData;
			bool m_SwapChainImageRetrieved{ false };

			uint32_t m_ImageStateCacheSize{ 0 };

			static constexpr vk::AccessFlags2 s_WriteFlagMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
																vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderStorageWrite |
																vk::AccessFlagBits2::eVideoDecodeWriteKHR | vk::AccessFlagBits2::eVideoEncodeWriteKHR | vk::AccessFlagBits2::eShaderTileAttachmentWriteQCOM |
																vk::AccessFlagBits2::eTransformFeedbackWriteEXT | vk::AccessFlagBits2::eTransformFeedbackWriteEXT | vk::AccessFlagBits2::eCommandPreprocessWriteEXT |
																vk::AccessFlagBits2::eCommandPreprocessWriteNV | vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureWriteNV | vk::AccessFlagBits2::eMicromapWriteEXT |
																vk::AccessFlagBits2::eOpticalFlowWriteNV | vk::AccessFlagBits2::eDataGraphWriteARM;
		};

		class Pass {
		public:
			Pass() {
				m_Writes.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_Reads.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_Consumers.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_ImageBarriers.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_BufferBarriers.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
			}

			void Writes(const GraphResourceHandle<ScratchBufferTag> resource, const BufferUsage& usage) {
				m_Writes.emplace_back(Internal::PassResourceDependency{ resource, Internal::ResourceUsage(usage) });
			}

			void Writes(const GraphResourceHandle<VulkanImage> resource, const ImageUsage& usage) {
				m_Writes.emplace_back(Internal::PassResourceDependency{ resource, Internal::ResourceUsage(usage) });
			}

			void Reads(const GraphResourceHandle<ScratchBufferTag> resource, const BufferUsage& usage) {
				m_Reads.emplace_back(Internal::PassResourceDependency{ resource, Internal::ResourceUsage(usage) });
			}

			void Reads(const GraphResourceHandle<VulkanImage> resource, const ImageUsage& usage) {
				m_Reads.emplace_back(Internal::PassResourceDependency{ resource, Internal::ResourceUsage(usage) });
			}

			void Reads([[maybe_unused]] const GraphResourceHandle<UploadBufferTag> resource) {}

			template<typename T>
			void Reads([[maybe_unused]] const GraphResourceHandle<VulkanDynamicVector<T>> resource) {}

			template<typename T>
			void Reads([[maybe_unused]] const GraphResourceHandle<VulkanFlatSlotMap<T>> resource) {}

			void AssignWork(std::function<void(vk::CommandBuffer cmb, RenderGraphResourceRegistry& registry)> work) {
				m_Work = std::move(work);
			}

		protected:
			friend RenderGraph;
			friend class RenderGraphPassRegistry;

			void AddProducer() {
				m_ProducersCount++;
			}

			void AddConsumer(const Internal::PassHandle consumer) {
				m_Consumers.emplace_back(consumer);
			}

			void AddImageBarrier(const vk::ImageMemoryBarrier2& barrier) {
				m_ImageBarriers.emplace_back(barrier);
			}

			void AddImageBarriers(std::vector<vk::ImageMemoryBarrier2>& barriers) {
				std::ranges::move(barriers, std::back_inserter(m_ImageBarriers));
			}

			void AddBufferBarriers(std::vector<vk::BufferMemoryBarrier2>& barriers) {
				std::ranges::move(barriers, std::back_inserter(m_BufferBarriers));
			}

			void AddBufferBarrier(const vk::BufferMemoryBarrier2& barrier) {
				m_BufferBarriers.emplace_back(barrier);
			}

			explicit Pass(const char* name) {
				m_Name = name;
			}

			void Reset(const char* name) {
				m_Name = name;
				m_Work = nullptr;
				m_Degree = 0;
				m_ProducersCount = 0;

				m_Writes.clear();
				m_Reads.clear();
				m_Consumers.clear();
				m_ImageBarriers.clear();
				m_BufferBarriers.clear();
			}

			const char* m_Name{ "" };

			std::function<void(vk::CommandBuffer cmb, RenderGraphResourceRegistry& registry)> m_Work;

			std::vector<Internal::PassResourceDependency> m_Writes;
			std::vector<Internal::PassResourceDependency> m_Reads;
			std::vector<Internal::PassHandle> m_Consumers;
			std::vector<vk::ImageMemoryBarrier2> m_ImageBarriers;
			std::vector<vk::BufferMemoryBarrier2> m_BufferBarriers;

			uint8_t m_ProducersCount{ 0 };

			uint64_t m_Degree{ 0 };
			Internal::PassHandle m_SelfHandle{ UINT32_MAX };
		};

		class RenderGraphPassRegistry {
		public:
			RenderGraphPassRegistry() {
				m_Passes.resize(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
				m_SortedPassOrder.reserve(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
				m_ResourceInPassSet.reserve(CORI_RENDER_GRAPH_PER_PASS_UNIQUE_RESOURCES_SET_INLINE_SIZE);
				m_ProducersSet.reserve(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
			}

			void Reset() {
				m_PassesSize = 0;
				m_SortedPassOrder.clear();
			}

		protected:
			friend RenderGraph;

			Pass& GetPass(const Internal::PassHandle handle) {
				return m_Passes[handle];
			}

			Pass& GetPass() {
				if (m_PassesSize >= m_Passes.size()) {
					m_Passes.resize(m_Passes.size() * 1.5f);
				}

				Pass& pass = m_Passes[m_PassesSize];
				pass.m_SelfHandle = m_PassesSize;
				m_PassesSize++;

				return pass;
			}

			void AddSortedPass(Internal::PassHandle handle) {
				m_SortedPassOrder.emplace_back(handle);
			}

			void AddResourceToSet(Internal::GraphResourceHandleBase handle) {
				for (auto& resource : m_ResourceInPassSet) {
					if (resource == handle) {
						return;
					}
				}

				m_ResourceInPassSet.emplace_back(handle);
			}

			void AddPassToSet(Internal::PassHandle handle) {
				for (auto& pass : m_ProducersSet) {
					if (pass == handle) {
						return;
					}
				}

				m_ProducersSet.emplace_back(handle);
			}

		private:

			std::vector<Pass> m_Passes;
			std::vector<Internal::PassHandle> m_SortedPassOrder;
			std::vector<Internal::PassHandle> m_ZeroInDegreeQueue;
			std::vector<Internal::GraphResourceHandleBase> m_ResourceInPassSet;
			std::vector<Internal::PassHandle> m_ProducersSet;

			uint32_t m_PassesSize{ 0 };
		};

		class RenderGraph {
		public:
			RenderGraph(RenderGraphPassRegistry& passRegistry, RenderGraphResourceRegistry& resourceRegistry) {
				m_ResourceRegistry = &resourceRegistry;
				m_PassRegistry = &passRegistry;
			}

			GraphResourceHandle<UploadBufferTag> ImportBuffer(const VulkanVirtualBuffer& buffer, const char* name) {
				Internal::ResourceNode node;
				node.name = name;
				node.origin = Internal::ResourceOrigin::Imported;
				node.resourceData.emplace<Internal::VirtualBufferResourceData>(Internal::VirtualBufferResourceData{ {}, buffer, { vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone } });

				return static_cast<GraphResourceHandle<UploadBufferTag>>(m_ResourceRegistry->AddNode(node));
			}

			template<typename T>
			GraphResourceHandle<VulkanDynamicVector<T>> ImportDynamicVector(const VulkanDynamicVector<T>& vector, const char* name) {
				Internal::ResourceNode node;
				node.name = name;
				node.origin = Internal::ResourceOrigin::Imported;
				node.resourceData.emplace<Internal::DynamicContainerResourceData>(static_cast<const void*>(&vector));

				return static_cast<GraphResourceHandle<VulkanDynamicVector<T>>>(m_ResourceRegistry->AddNode(node));
			}

			template<typename T, uint16_t REUSE_THRESHOLD = 64, bool ENABLE_VERSIONING = true, Core::IsVersionedHandle HandleT = Core::Handle<T>, typename ConstHandleT = Core::ConstHandle<T>>
			GraphResourceHandle<VulkanFlatSlotMap<T>> ImportFlatSlotMap(const VulkanFlatSlotMap<T, REUSE_THRESHOLD, ENABLE_VERSIONING, HandleT, ConstHandleT>& vector, const char* name) {
				Internal::ResourceNode node;
				node.name = name;
				node.origin = Internal::ResourceOrigin::Imported;
				node.resourceData.emplace<Internal::DynamicContainerResourceData>(static_cast<const void*>(&vector));

				return static_cast<GraphResourceHandle<VulkanFlatSlotMap<T>>>(m_ResourceRegistry->AddNode(node));
			}

			GraphResourceHandle<ScratchBufferTag> CreateBuffer(const ScratchBufferCreateInfo& createInfo, const char* name) {
				Internal::ResourceNode node;
				node.name = name;
				node.origin = Internal::ResourceOrigin::Created;
				node.resourceData.emplace<Internal::VirtualBufferResourceData>(Internal::VirtualBufferResourceData{ createInfo, {}, {} });

				return static_cast<GraphResourceHandle<ScratchBufferTag>>(m_ResourceRegistry->AddNode(node));
			}

			GraphResourceHandle<VulkanImage> CreateImage(const VulkanImage::CreateInfo& createInfo, const char* name) {
				Internal::ResourceNode node;
				node.name = name;
				node.origin = Internal::ResourceOrigin::Created;

				node.resourceData.emplace<Internal::ImageResourceData>(Internal::ImageResourceData{ { *createInfo.imageCreateInfo, *createInfo.allocationCreateInfo, createInfo.name }, {}, m_ResourceRegistry->GetFreeImageState() });

				return static_cast<GraphResourceHandle<VulkanImage>>(m_ResourceRegistry->AddNode(node));
			}

			Pass& CreatePass(const char* name) {
				Pass& pass = m_PassRegistry->GetPass();
				pass.Reset(name);

				return pass;
			}

			void Compile(const uint64_t currentFrameIndex, const uint32_t dstFrameInFlight) {
				CORI_PROFILE_FUNCTION();
				{
					CORI_PROFILE_SCOPE("DAG Build");
					#if 0
					for (auto& pass : std::ranges::subrange(m_PassRegistry->m_Passes.begin(), m_PassRegistry->m_Passes.begin() + m_PassRegistry->m_PassesSize)) {
						for (const auto& [resourceHandle, usage] : std::ranges::subrange(pass.m_Reads.begin(), pass.m_Reads.begin() + pass.m_ReadsSize)) {
							auto& resourceNode = m_ResourceRegistry->GetNode(resourceHandle);
							if (resourceNode.lastProducer != UINT32_MAX) {
								Pass& producer = m_PassRegistry->GetPass(resourceNode.lastProducer);
								pass.AddProducer(resourceNode.lastProducer, resourceHandle);
								producer.AddConsumer(pass.m_SelfHandle, resourceHandle);
							}
						}

						for (const auto& [resourceHandle, usage] : std::ranges::subrange(pass.m_Writes.begin(), pass.m_Writes.begin() + pass.m_WritesSize)) {
							auto& resourceNode = m_ResourceRegistry->GetNode(resourceHandle);
							if (resourceNode.lastProducer != UINT32_MAX) {
								if (pass.m_SelfHandle != resourceNode.lastProducer) {
									Pass& producer = m_PassRegistry->GetPass(resourceNode.lastProducer);
									pass.AddProducer(resourceNode.lastProducer, resourceHandle);
									producer.AddConsumer(pass.m_SelfHandle, resourceHandle);
								}
							}

							resourceNode.lastProducer = pass.m_SelfHandle;
						}
					}
					#else
					for (auto& pass : std::ranges::subrange(m_PassRegistry->m_Passes.begin(), m_PassRegistry->m_Passes.begin() + m_PassRegistry->m_PassesSize)) {
						for (const auto& readDep : pass.m_Reads) {
							Internal::GraphResourceHandleBase handle = readDep.resource;
							auto& node = m_ResourceRegistry->GetNode(handle);

							if (node.lastProducer != UINT32_MAX) {
								m_PassRegistry->AddPassToSet(node.lastProducer);
							}
						}

						for (const auto& writeDep : pass.m_Writes) {
							Internal::GraphResourceHandleBase handle = writeDep.resource;
							auto& node = m_ResourceRegistry->GetNode(handle);

							if (node.lastProducer != UINT32_MAX) {
								m_PassRegistry->AddPassToSet(node.lastProducer);
							}
						}

						for (auto handle : m_PassRegistry->m_ProducersSet) {
							if (handle != pass.m_SelfHandle) {
								pass.AddProducer();
								m_PassRegistry->GetPass(handle).AddConsumer(pass.m_SelfHandle);
							}
						}

						for (const auto& writeDep : pass.m_Writes) {
							m_ResourceRegistry->GetNode(writeDep.resource).lastProducer = pass.m_SelfHandle;
						}

						m_PassRegistry->m_ProducersSet.clear();
					}

					#endif
				}

				{
					CORI_PROFILE_SCOPE("Kahn's topological sort.");

					for (auto& pass : std::ranges::subrange(m_PassRegistry->m_Passes.begin(), m_PassRegistry->m_Passes.begin() + m_PassRegistry->m_PassesSize)) {
						uint64_t degree = pass.m_ProducersCount;
						pass.m_Degree = degree;

						if (degree == 0) {
							m_PassRegistry->m_ZeroInDegreeQueue.push_back(pass.m_SelfHandle);
						}
					}

					while (!m_PassRegistry->m_ZeroInDegreeQueue.empty()) {
						Pass& pass = m_PassRegistry->GetPass(m_PassRegistry->m_ZeroInDegreeQueue.front());
						m_PassRegistry->m_ZeroInDegreeQueue.erase(m_PassRegistry->m_ZeroInDegreeQueue.begin());
						m_PassRegistry->AddSortedPass( pass.m_SelfHandle);

						for (const auto& consumerDep : pass.m_Consumers) {
							Pass& consumerPass = m_PassRegistry->GetPass(consumerDep);
							consumerPass.m_Degree--;
							if (consumerPass.m_Degree == 0) {
								m_PassRegistry->m_ZeroInDegreeQueue.push_back(consumerPass.m_SelfHandle);
							}
						}
					}

					CORI_CORE_ASSERT(m_PassRegistry->m_SortedPassOrder.size() == m_PassRegistry->m_PassesSize, "Error when compiling the render graph, graph has a cycle.")
				}

				{
					CORI_PROFILE_SCOPE("Resource creation and acquisition.");
					for (auto& node : m_ResourceRegistry->m_Nodes) {
						if (node.origin == Internal::ResourceOrigin::Created) {
							if (std::holds_alternative<Internal::ImageResourceData>(node.resourceData)) {
								auto& data = std::get<Internal::ImageResourceData>(node.resourceData);

								data.image = m_ResourceRegistry->GetPooledImage(data.createInfo, currentFrameIndex);
								data.stateHandle = m_ResourceRegistry->GetFreeImageState();
							}
							else if (std::holds_alternative<Internal::VirtualBufferResourceData>(node.resourceData)) {
								auto& data = std::get<Internal::VirtualBufferResourceData>(node.resourceData);

								data.virtualBuffer = VulkanVirtualBufferAllocator::CreateVirtualScratchBuffer(data.createInfo.size, data.createInfo.alignment, dstFrameInFlight, data.createInfo.name);
							}
						}
					}
				}

				{
					CORI_PROFILE_SCOPE("Barrier generation");
					for (auto& passHandle : m_PassRegistry->m_SortedPassOrder) {
						Pass& pass = m_PassRegistry->GetPass(passHandle);

						auto ProcessResource = [&](const Internal::PassResourceDependency& dep) {
							Internal::ResourceNode& node = m_ResourceRegistry->GetNode(dep.resource);

							if (std::holds_alternative<Internal::ImageResourceData>(node.resourceData)) {
								auto& data = std::get<Internal::ImageResourceData>(node.resourceData);
								auto& state = m_ResourceRegistry->GetImageState(data.stateHandle);
								auto& image = data.image;
								auto& range = dep.usage.subrange;
								auto imageAspect = image.GetAspectMask();

								bool isFullImage = range.baseMipLevel == 0 && range.levelCount == image.m_MipLevels && range.baseArrayLayer == 0 && range.layerCount == image.m_ArrayLayers && !(~imageAspect & range.aspectMask);

								if (isFullImage) {
									if (state.isUniform) {
										if (state.globalState != dep.usage.desiredState) {

											vk::ImageMemoryBarrier2 barrier{
												.srcStageMask = state.globalState.stageMask,
												.srcAccessMask = state.globalState.accessMask,
												.dstStageMask = dep.usage.desiredState.stageMask,
												.dstAccessMask = dep.usage.desiredState.accessMask,
												.oldLayout = state.globalState.imageLayout,
												.newLayout = dep.usage.desiredState.imageLayout,
												.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												.image = image.m_Image,
												.subresourceRange = range
											};

											pass.AddImageBarrier(barrier);

											state.globalState = dep.usage.desiredState;
										}

										return;
									}
								}

								uint8_t planeCount = 0;
								planeCount += static_cast<bool>(imageAspect & vk::ImageAspectFlagBits::eColor);
								planeCount += static_cast<bool>(imageAspect & vk::ImageAspectFlagBits::eDepth);
								planeCount += static_cast<bool>(imageAspect & vk::ImageAspectFlagBits::eStencil);

								if (state.isUniform) {
									state.subresourceStates.assign(planeCount * image.m_ArrayLayers * image.m_MipLevels, state.globalState);
									state.isUniform = false;
								}

								constexpr std::array<vk::ImageAspectFlagBits, 3> possiblePlanes = { vk::ImageAspectFlagBits::eColor, vk::ImageAspectFlagBits::eDepth, vk::ImageAspectFlagBits::eStencil };

								uint8_t planeCounter = 0;
								for (auto planeAspect : possiblePlanes) {
									if (!(imageAspect & planeAspect)) {
										continue;
									}

									uint32_t planeOffset = planeCounter++ * image.m_ArrayLayers * image.m_MipLevels;

									for (uint32_t layer = range.baseArrayLayer; layer < range.baseArrayLayer + range.layerCount; ++layer) {
										uint32_t layerOffset = layer * image.m_ArrayLayers;
										Internal::ResourceState batchSrcState = state.subresourceStates[planeOffset + layerOffset + range.baseMipLevel];
										uint32_t currentStartMip = range.baseMipLevel;
										uint32_t mipCount = 0;
										bool batchActive = false;

										for (uint32_t mip = range.baseMipLevel; mip < range.baseMipLevel + range.levelCount; ++mip) {
											uint32_t index = planeOffset + layerOffset + mip;
											Internal::ResourceState& currentState = state.subresourceStates[index];

											if (currentState != dep.usage.desiredState) {
												if (!batchActive) {
													batchSrcState = currentState;
													currentStartMip = mip;
													mipCount = 1;
													batchActive = true;
												}
												else if (currentState == batchSrcState) {
													mipCount++;
												}
												else {
													vk::ImageSubresourceRange batchRange{
														.aspectMask = planeAspect,
														.baseMipLevel = currentStartMip,
														.levelCount = mipCount,
														.baseArrayLayer = layer,
														.layerCount = 1
													};

													vk::ImageMemoryBarrier2 barrier{
														.srcStageMask = batchSrcState.stageMask,
														.srcAccessMask = batchSrcState.accessMask,
														.dstStageMask = dep.usage.desiredState.stageMask,
														.dstAccessMask = dep.usage.desiredState.accessMask,
														.oldLayout = batchSrcState.imageLayout,
														.newLayout = dep.usage.desiredState.imageLayout,
														.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
														.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
														.image = image.m_Image,
														.subresourceRange = batchRange
													};

													pass.AddImageBarrier(barrier);

													batchSrcState = currentState;
													currentStartMip = mip;
													mipCount = 1;
												}

												currentState = dep.usage.desiredState;
											}
											else {
												if (batchActive) {
													vk::ImageSubresourceRange batchRange{
														.aspectMask = planeAspect,
														.baseMipLevel = currentStartMip,
														.levelCount = mipCount,
														.baseArrayLayer = layer,
														.layerCount = 1
													};

													vk::ImageMemoryBarrier2 barrier{
														.srcStageMask = batchSrcState.stageMask,
														.srcAccessMask = batchSrcState.accessMask,
														.dstStageMask = dep.usage.desiredState.stageMask,
														.dstAccessMask = dep.usage.desiredState.accessMask,
														.oldLayout = batchSrcState.imageLayout,
														.newLayout = dep.usage.desiredState.imageLayout,
														.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
														.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
														.image = image.m_Image,
														.subresourceRange = batchRange
													};

													pass.AddImageBarrier(barrier);

													batchActive = false;
												}
											}
										}

										if (batchActive) {
											vk::ImageSubresourceRange batchRange{
												.aspectMask = planeAspect,
												.baseMipLevel = currentStartMip,
												.levelCount = mipCount,
												.baseArrayLayer = layer,
												.layerCount = 1
											};

											vk::ImageMemoryBarrier2 barrier{
												.srcStageMask = batchSrcState.stageMask,
												.srcAccessMask = batchSrcState.accessMask,
												.dstStageMask = dep.usage.desiredState.stageMask,
												.dstAccessMask = dep.usage.desiredState.accessMask,
												.oldLayout = batchSrcState.imageLayout,
												.newLayout = dep.usage.desiredState.imageLayout,
												.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												.image = image.m_Image,
												.subresourceRange = batchRange
											};

											pass.AddImageBarrier(barrier);
										}
									}
								}


							}
							else if (std::holds_alternative<Internal::VirtualBufferResourceData>(node.resourceData)) {
								auto& data = std::get<Internal::VirtualBufferResourceData>(node.resourceData);

								vk::BufferMemoryBarrier2 barrier{
									.srcStageMask = data.state.stageMask,
									.srcAccessMask = data.state.accessMask,
									.dstStageMask = dep.usage.desiredState.stageMask,
									.dstAccessMask = dep.usage.desiredState.accessMask,
									.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.buffer = data.virtualBuffer.GetHeapHandle(),
									.offset = data.virtualBuffer.GetStartOffset(),
									.size = data.virtualBuffer.GetSize()
								};

								data.state = dep.usage.desiredState;

								pass.AddBufferBarrier(barrier);
							}
						};

						for (const auto& write : pass.m_Writes) {
							ProcessResource(write);
						}

						for (const auto& read : pass.m_Reads) {
							ProcessResource(read);
						}

						m_PassRegistry->m_ResourceInPassSet.clear();
					}
				}
			}

			void Execute(vk::CommandBuffer cmb) {
				for (const auto& passHandle : m_PassRegistry->m_SortedPassOrder) {
					const Pass& pass = m_PassRegistry->GetPass(passHandle);

					if (m_ResourceRegistry->m_SwapChainImageRetrieved) {
						break;
					}

					CORI_PROFILE_SCOPE_DYNAMIC_NAME(pass.m_Name);
					CORI_PROFILE_GPU_ZONE_DYNAMIC_NAME_CP(Cori::ProfileParts::RenderingLoop, VulkanEngine::GetGraphicsGPUProfilerContext(), cmb, pass.m_Name, Cori::ProfileColors::GPUPass);
					CORI_VK_LABEL(cmb, pass.m_Name, DebugLabelColors::Pass);
					CORI_VK_DL_MARKER_SCOPE(cmb, pass.m_Name);

					if (pass.m_ImageBarriers.size() > 0 || pass.m_BufferBarriers.size() > 0) {
						vk::DependencyInfo depInfo {
							.bufferMemoryBarrierCount = static_cast<uint32_t>(pass.m_BufferBarriers.size()),
							.pBufferMemoryBarriers = pass.m_BufferBarriers.data(),
							.imageMemoryBarrierCount = static_cast<uint32_t>(pass.m_ImageBarriers.size()),
							.pImageMemoryBarriers = pass.m_ImageBarriers.data()
						};

						CORI_VK_LABEL_INSERT_F(cmb, DebugLabelColors::Barrier, "Barriers: {} image, {} buffer", pass.m_ImageBarriers.size(), pass.m_BufferBarriers.size());

						cmb.pipelineBarrier2(depInfo);
					}

					if (pass.m_Work) {
						pass.m_Work(cmb, *m_ResourceRegistry);
					}
				}
			}

			void PrintGraph() const {
				#if 0
				std::cout << "--- Render Graph ---" << std::endl;
				std::cout << "Execution Order (" << m_PassRegistry->m_SortedPassOrderSize << " passes):" << std::endl;

				uint32_t i = 0;
				for (auto& passHandle : std::ranges::subrange(m_PassRegistry->m_SortedPassOrder.begin(), m_PassRegistry->m_SortedPassOrder.begin() + m_PassRegistry->m_SortedPassOrderSize)) {
					Pass& pass = m_PassRegistry->GetPass(passHandle);
					std::cout << "  [" << i << "] " << pass.m_Name << std::endl;
					i++;
				}
				std::cout << "--------------------" << std::endl;

				i = 0;
				for (auto& passHandle : std::ranges::subrange(m_PassRegistry->m_SortedPassOrder.begin(), m_PassRegistry->m_SortedPassOrder.begin() + m_PassRegistry->m_SortedPassOrderSize)) {
					Pass& pass = m_PassRegistry->GetPass(passHandle);

					std::cout << "\nPASS [" << i << "]: " << pass.m_Name << std::endl;
					std::cout << "  Producers (" << static_cast<uint32_t>(pass.m_ProducersCount) << " dependencies):" << std::endl;
					if (pass.m_ProducersCount == 0) {
						std::cout << "    - None (Root Pass)" << std::endl;
					}
					else {
						for (const auto& dep : std::ranges::subrange(pass.m_Producers.begin(), pass.m_Producers.begin() + pass.m_ProducersCount)) {
							const ResourceNode& res_node = m_ResourceRegistry->GetNode(dep.resource);
							std::cout << "    - Depends on '" << m_PassRegistry->GetPass(dep.otherPass).m_Name
								<< "' because of resource '" << res_node.name << "'" << std::endl;
						}
					}

					std::cout << "  Pre-Pass Barriers ("
						<< static_cast<uint32_t>(pass.m_BufferBarriersSize + pass.m_ImageBarriersSize)
						<< " total):" << std::endl;

					if (pass.m_BufferBarriersSize == 0 && pass.m_ImageBarriersSize == 0) {
						std::cout << "    - None" << std::endl;
					}

					// Print Buffer Barriers
					for (const auto& barrier : std::ranges::subrange(pass.m_BufferBarriers.begin(), pass.m_BufferBarriers.begin() + pass.m_BufferBarriersSize)) {
						ResourceHandle handle = 0;
						std::cout << "    - BUFFER BARRIER for [Resource Handle: " << "lookup_needed" << "]" << std::endl;
						std::cout << "        - Src Stage: " << vk::to_string(barrier.srcStageMask) << std::endl;
						std::cout << "        - Src Access: " << vk::to_string(barrier.srcAccessMask) << std::endl;
						std::cout << "        - Dst Stage: " << vk::to_string(barrier.dstStageMask) << std::endl;
						std::cout << "        - Dst Access: " << vk::to_string(barrier.dstAccessMask) << std::endl;
					}

					// Print Image Barriers
					for (const auto& barrier : std::ranges::subrange(pass.m_ImageBarriers.begin(), pass.m_ImageBarriers.begin() + pass.m_ImageBarriersSize)) {
						// This is a tricky part. To get the resource name from a VkImage handle,
						// you'd need to iterate through your registry.
						std::string res_name = "[Unknown Image]";
						for (const auto& node : std::ranges::subrange(m_ResourceRegistry->m_Nodes.begin(), m_ResourceRegistry->m_Nodes.begin() + m_ResourceRegistry->m_NodesSize)) {
							// This assumes your registry's resource nodes have the physical handle assigned.
							//if (node.type == ResourceType::Image && node.physical_image_handle == barrier.image) {
							//	res_name = node.name;
							//	break;
							//}
						}

						std::cout << "    - IMAGE BARRIER for '" << res_name << "'" << std::endl;
						std::cout << "        - Src Stage: " << vk::to_string(barrier.srcStageMask) << std::endl;
						std::cout << "        - Src Access: " << vk::to_string(barrier.srcAccessMask) << std::endl;
						std::cout << "        - Old Layout: " << vk::to_string(barrier.oldLayout) << std::endl;
						std::cout << "        - Dst Stage: " << vk::to_string(barrier.dstStageMask) << std::endl;
						std::cout << "        - Dst Access: " << vk::to_string(barrier.dstAccessMask) << std::endl;
						std::cout << "        - New Layout: " << vk::to_string(barrier.newLayout) << std::endl;
					}
					i++;
				}

				if (m_FinalOutputBarrier) {
					std::string res_name = "[Unknown Image]";
					std::cout << "\nFINAL Post-Graph Barrier:" << std::endl;
					std::cout << "    - IMAGE BARRIER for '" << res_name << "'" << std::endl;
					std::cout << "        - Src Stage: " << vk::to_string(m_FinalOutputBarrier.srcStageMask) << std::endl;
					std::cout << "        - ... (and so on for all fields)" << std::endl;
				}

				std::cout << "\n--- End of Graph ---" << std::endl;
				#endif
			}

		private:
			RenderGraphResourceRegistry* m_ResourceRegistry;
			RenderGraphPassRegistry* m_PassRegistry;
		};
	}
}
