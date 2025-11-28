#pragma once
#include <gch/small_vector.hpp>
#include "Vulkan/VulkanEngine.hpp"
#include "Vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "Vulkan/VulkanResourceTracker.hpp"
#include "Utility/StringHash.hpp"
#include "ResourceType.hpp"

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

		struct BufferCreateInfo {
			vk::BufferCreateInfo bufferCreateInfo;
			vma::AllocationCreateInfo allocationCreateInfo;
			const char* name = "";
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
		};

		struct PooledBufferDescription {
			explicit PooledBufferDescription(const BufferCreateInfo& fullInfo) {
				flags = fullInfo.bufferCreateInfo.flags;
				size = fullInfo.bufferCreateInfo.size;
				usage = fullInfo.bufferCreateInfo.usage;
			}

			vk::BufferCreateFlags flags;
			vk::DeviceSize size;
			vk::BufferUsageFlags usage;

			bool operator==(const PooledBufferDescription& other) const = default;

			auto operator<=>(const PooledBufferDescription& other) const = default;
		};

		struct PooledImage {
			VulkanImage image;
			uint32_t framesAgoUsed;
		};

		struct PooledBuffer {
			VulkanBuffer buffer;
			uint32_t framesAgoUsed;
		};

		enum class ResourceOrigin {
			Undefined,
			Imported,
			Created
		};

		using ResourceHandle = uint32_t;
		using PassHandle = uint32_t;

		struct PassPassDependency {
			PassHandle otherPass;
			ResourceHandle resource;
		};

		struct BufferRange {
			uint32_t offset{ 0 };
			uint32_t size{ 0 };
		};

		struct ResourceUsage {
			ResourceState desiredState;

			std::variant<std::monostate, vk::ImageSubresourceRange, BufferRange> subrange;

			//union {
			//	BufferRange bufferRange;
			//	vk::ImageSubresourceRange imageRange;
			//};
		};

		struct PassResourceDependency {
			ResourceHandle resource;
			ResourceUsage usage;
		};

		struct ResourceNode {
			const char* name;
			ResourceOrigin origin;
			ResourceType type;
			PassHandle lastProducer{ UINT32_MAX };
			bool resourceAllocated{ false };

			//union {
			//	ImageCreateInfo imageInfo;
			//	BufferCreateInfo bufferInfo;
			//};

			std::variant<std::monostate, ImageCreateInfo, BufferCreateInfo> createInfo;
			std::variant<std::monostate, VulkanImage, VulkanBuffer> resource;

			//union {
			//	VulkanBuffer buffer;
			//	VulkanImage image;
			//};
		};

		struct SwapChainImageData {
			vk::Image swapChainImage = nullptr;
			vk::ImageView swapChainImageView = nullptr;
			vk::Extent2D imageExtent;
		};

		class RenderGraphResourceRegistry {
		public:
			RenderGraphResourceRegistry() {
				m_Nodes.resize(CORI_RENDER_GRAPH_RESOURCE_REGISTRY_INLINE_SIZE);
				m_ImagesToFree.reserve(CORI_RENDER_GRAPH_RESOURCE_REGISTRY_INLINE_SIZE);
				m_BuffersToFree.reserve(CORI_RENDER_GRAPH_RESOURCE_REGISTRY_INLINE_SIZE);
			}

			void Reset() {
				m_SwapChainImageData = {};
				m_SwapChainImageRetrieved = false;
				m_NodesSize = 0;
			}

			void UpdateResourceStates() {
				for (auto& pooledImage : m_ImagePool | std::views::values) {
					pooledImage.framesAgoUsed++;
				}

				for (auto& pooledBuffer : m_BufferPool | std::views::values) {
					pooledBuffer.framesAgoUsed++;
				}
			}

			void ClearStalePooledResources() {
				m_ImagesToFree.clear();
				m_BuffersToFree.clear();
				for (auto it = m_ImagePool.begin(); it != m_ImagePool.end(); ++it) {
					if (it->second.framesAgoUsed > CORI_STALE_RESOURCE_LIFETIME) {
						m_ImagesToFree.push_back(it);
						it->second.image.Destroy();
					}
				}

				for (auto it = m_BufferPool.begin(); it != m_BufferPool.end(); ++it) {
					if (it->second.framesAgoUsed > CORI_STALE_RESOURCE_LIFETIME) {
						m_BuffersToFree.push_back(it);
						it->second.buffer.Destroy();
					}
				}

				for (auto it : m_ImagesToFree) {
					m_ImagePool.erase(it);
				}

				for (auto it : m_BuffersToFree) {
					m_BufferPool.erase(it);
				}
			}

			ResourceNode& GetNode(const ResourceHandle handle) {
				return m_Nodes[handle];
			}

			ResourceHandle AddNode(ResourceNode&& node) {
				if (m_NodesSize >= m_Nodes.size()) {
					m_Nodes.resize(m_Nodes.size() * 1.5f);
				}

				m_Nodes[m_NodesSize] = std::move(node);

				return m_NodesSize++;
			}

			ResourceHandle AddNode(const ResourceNode& node) {
				if (m_NodesSize >= m_Nodes.size()) {
					m_Nodes.resize(m_Nodes.size() * 1.5f);
				}

				m_Nodes[m_NodesSize] = node;

				return m_NodesSize++;
			}

			void RegisterSwapChainImage(SwapChainImageData&& data) {
				m_SwapChainImageData = std::move(data);
			}

			void RegisterSwapChainImage(const SwapChainImageData& data) {
				m_SwapChainImageData = data;
			}

			SwapChainImageData& GetSwapChainImageData() {
				m_SwapChainImageRetrieved = true;
				return m_SwapChainImageData;
			}

			//TODO: add GetImage and GetBuffer

			std::vector<ResourceNode> m_Nodes;

			std::map<PooledImageDescription, PooledImage> m_ImagePool;
			std::map<PooledBufferDescription, PooledBuffer> m_BufferPool;
			std::vector<std::map<PooledImageDescription, PooledImage>::iterator> m_ImagesToFree;
			std::vector<std::map<PooledBufferDescription, PooledBuffer>::iterator> m_BuffersToFree;


			SwapChainImageData m_SwapChainImageData;
			bool m_SwapChainImageRetrieved{ false };
			uint32_t m_NodesSize{ 0 };

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
				m_Writes.resize(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_Reads.resize(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				//m_Producers.resize(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_Consumers.resize(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_ImageBarriers.resize(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
				m_BufferBarriers.resize(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
			}


			explicit Pass(const char* name) {
				m_Name = name;
			}

			void Reset(const char* name) {
				m_Name = name;
				m_Work = nullptr;
				m_Degree = 0;
				m_WritesSize = 0;
				m_ReadsSize = 0;
				m_ProducersCount = 0;
				m_ConsumersSize = 0;
				m_ImageBarriersSize = 0;
				m_BufferBarriersSize = 0;
			}

			void Writes(const ResourceHandle resource, const ResourceUsage& usage) {
				if (m_WritesSize >= m_Writes.size()) {
					m_Writes.resize(m_Writes.size() * 1.5f);
				}

				m_Writes[m_WritesSize++] = { resource, usage };
			}

			void Reads(const ResourceHandle resource, const ResourceUsage& usage) {
				if (m_ReadsSize >= m_Reads.size()) {
					m_Reads.resize(m_Reads.size() * 1.5f);
				}

				m_Reads[m_ReadsSize++] = { resource, usage };
			}


			//void AddProducer(const PassHandle producer, const ResourceHandle resource) {
			//	if (m_ProducersCount >= m_Producers.size()) {
			//		m_Producers.resize(m_Producers.size() * 1.5f);
			//	}
//
			//	m_Producers[m_ProducersCount++] = { producer, resource };
			//}

			void AddProducer() {
				m_ProducersCount++;
			}

			void AddConsumer(const PassHandle consumer) {
				if (m_ConsumersSize >= m_Consumers.size()) {
					m_Consumers.resize(m_Consumers.size() * 1.5f);
				}

				m_Consumers[m_ConsumersSize++] = { consumer };
			}

			void AddImageBarrier(vk::ImageMemoryBarrier2&& barrier) {
				if (m_ImageBarriersSize >= m_ImageBarriers.size()) {
					m_ImageBarriers.resize(m_ImageBarriers.size() * 1.5f);
				}

				m_ImageBarriers[m_ImageBarriersSize++] = barrier;
			}

			void AddBufferBarrier(vk::BufferMemoryBarrier2&& barrier) {
				if (m_BufferBarriersSize >= m_BufferBarriers.size()) {
					m_BufferBarriers.resize(m_BufferBarriers.size() * 1.5f);
				}

				m_BufferBarriers[m_BufferBarriersSize++] = barrier;
			}

			//TODO: add an ability to add multiple barriers at once.

			void AddBufferBarriers(void* data, size_t bufferAmount) {
				// check if we have enough space
				// memcpy from data to m_BufferBarriers + m_BufferBarriersSize at size 'bufferAmount'

				// or do some range/view/iterator smart thing idk, fuck around and find out i guess, oki bye)
			}

			void AssignWork(std::function<void(vk::CommandBuffer cmb, const RenderGraphResourceRegistry& registry)> work) {
				m_Work = std::move(work);
			}

		public:
			const char* m_Name{ "" };

			friend class RenderGraph;
			std::function<void(vk::CommandBuffer cmb, const RenderGraphResourceRegistry& registry)> m_Work;

			std::vector<PassResourceDependency> m_Writes;
			std::vector<PassResourceDependency> m_Reads;
			//std::vector<PassPassDependency> m_Producers;
			std::vector<PassHandle> m_Consumers;
			std::vector<vk::ImageMemoryBarrier2> m_ImageBarriers;
			std::vector<vk::BufferMemoryBarrier2> m_BufferBarriers;

			uint64_t m_Degree{ 0 };

			uint8_t m_WritesSize{ 0 };
			uint8_t m_ReadsSize{ 0 };
			uint8_t m_ProducersCount{ 0 };
			uint8_t m_ConsumersSize{ 0 };
			uint8_t m_ImageBarriersSize{ 0 };
			uint8_t m_BufferBarriersSize{ 0 };
			PassHandle m_SelfHandle{ UINT32_MAX };
		};

		class RenderGraphPassRegistry {
		public:
			RenderGraphPassRegistry() {
				m_Passes.resize(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
				m_SortedPassOrder.resize(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
				m_ResourceInPassSet.resize(CORI_RENDER_GRAPH_PER_PASS_UNIQUE_RESOURCES_SET_INLINE_SIZE);
				m_ProducersSet.resize(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
			}

			void Reset() {
				m_PassesSize = 0;
				m_SortedPassOrderSize = 0;
			}

			Pass& GetPass(const PassHandle handle) {
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

			void AddSortedPass(PassHandle handle) {
				if (m_SortedPassOrderSize >= m_SortedPassOrder.size()) {
					m_SortedPassOrder.resize(m_SortedPassOrder.size() * 1.5f);
				}

				m_SortedPassOrder[m_SortedPassOrderSize++] = handle;
			}

			void AddResourceToSet(ResourceHandle handle) {
				for (size_t i = 0; i < m_ResourceInPassSetSize; ++i) {
					if (m_ResourceInPassSet[i] == handle) {
						return;
					}
				}

				if (m_ResourceInPassSetSize >= m_ResourceInPassSet.size()) {
					m_ResourceInPassSet.resize(m_ResourceInPassSet.size() * 2.0f);
				}

				m_ResourceInPassSet[m_ResourceInPassSetSize++] = handle;
			}

			void AddPassToSet(PassHandle handle) {
				for (size_t i = 0; i < m_ProducersSetSize; ++i) {
					if (m_ProducersSet[i] == handle) {
						return;
					}
				}

				if (m_ProducersSetSize >= m_ProducersSet.size()) {
					m_ProducersSet.resize(m_ProducersSet.size() * 2.0f);
				}

				m_ProducersSet[m_ProducersSetSize++] = handle;
			}

			std::vector<Pass> m_Passes;
			std::vector<PassHandle> m_SortedPassOrder;
			std::vector<PassHandle> m_ZeroInDegreeQueue;
			std::vector<ResourceHandle> m_ResourceInPassSet;
			std::vector<PassHandle> m_ProducersSet;

			uint32_t m_ResourceInPassSetSize{ 0 };
			uint32_t m_ProducersSetSize{ 0 };
			uint32_t m_SortedPassOrderSize{ 0 };
			uint32_t m_PassesSize{ 0 };
		};

		class RenderGraph {
		public:
			RenderGraph(RenderGraphPassRegistry& passRegistry, RenderGraphResourceRegistry& resourceRegistry) {
				m_ResourceRegistry = &resourceRegistry;
				m_PassRegistry = &passRegistry;
			}

			ResourceHandle ImportBuffer(const VulkanBuffer& buffer, const char* name) {
				ResourceNode node;
				node.name = name;
				node.type = ResourceType::Buffer;
				node.origin = ResourceOrigin::Imported;
				node.resourceAllocated = true;
				node.resource.emplace<VulkanBuffer>(buffer);

				return m_ResourceRegistry->AddNode(std::move(node));
			}

			ResourceHandle ImportImage(const VulkanImage& image, const char* name) {
				ResourceNode node;
				node.name = name;
				node.type = ResourceType::Image;
				node.origin = ResourceOrigin::Imported;
				node.resourceAllocated = true;
				node.resource.emplace<VulkanImage>(image);

				return m_ResourceRegistry->AddNode(std::move(node));
			}

			ResourceHandle CreateBuffer(const VulkanBuffer::CreateInfo& createInfo, const char* name) {
				ResourceNode node;
				node.name = name;
				node.type = ResourceType::Buffer;
				node.origin = ResourceOrigin::Created;
				node.createInfo.emplace<BufferCreateInfo>(
					*createInfo.bufferCreateInfo,
					*createInfo.allocationCreateInfo,
					createInfo.name
				);

				return m_ResourceRegistry->AddNode(std::move(node));
			}

			ResourceHandle CreateImage(const VulkanImage::CreateInfo& createInfo, const char* name) {
				ResourceNode node;
				node.name = name;
				node.type = ResourceType::Image;
				node.origin = ResourceOrigin::Created;
				node.createInfo.emplace<ImageCreateInfo>(
					*createInfo.imageCreateInfo,
					*createInfo.allocationCreateInfo,
					createInfo.name
				);

				return m_ResourceRegistry->AddNode(std::move(node));
			}

			Pass& CreatePass(const char* name) {
				Pass& pass = m_PassRegistry->GetPass();
				pass.Reset(name);

				return pass;
			}

			void Compile() {
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
						for (const auto& readDep : std::ranges::subrange(pass.m_Reads.begin(), pass.m_Reads.begin() + pass.m_ReadsSize)) {
							ResourceHandle handle = readDep.resource;
							auto& node = m_ResourceRegistry->GetNode(handle);

							if (node.lastProducer != UINT32_MAX) {
								m_PassRegistry->AddPassToSet(node.lastProducer);
							}
						}

						for (const auto& writeDep : std::ranges::subrange(pass.m_Writes.begin(), pass.m_Writes.begin() + pass.m_WritesSize)) {
							ResourceHandle handle = writeDep.resource;
							auto& node = m_ResourceRegistry->GetNode(handle);

							if (node.lastProducer != UINT32_MAX) {
								m_PassRegistry->AddPassToSet(node.lastProducer);
							}
						}

						for (PassHandle handle : std::ranges::subrange(m_PassRegistry->m_ProducersSet.begin(), m_PassRegistry->m_ProducersSet.begin() + m_PassRegistry->m_ProducersSetSize)) {
							if (handle != pass.m_SelfHandle) {
								pass.AddProducer();
								m_PassRegistry->GetPass(handle).AddConsumer(pass.m_SelfHandle);
							}
						}

						for (const auto& writeDep : std::ranges::subrange(pass.m_Writes.begin(), pass.m_Writes.begin() + pass.m_WritesSize)) {
							m_ResourceRegistry->GetNode(writeDep.resource).lastProducer = pass.m_SelfHandle;
						}

						m_PassRegistry->m_ProducersSetSize = 0;
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

						for (const auto& consumerDep : std::ranges::subrange(pass.m_Consumers.begin(), pass.m_Consumers.begin() + pass.m_ConsumersSize)) {
							Pass& consumerPass = m_PassRegistry->GetPass(consumerDep);
							consumerPass.m_Degree--;
							if (consumerPass.m_Degree == 0) {
								m_PassRegistry->m_ZeroInDegreeQueue.push_back(consumerPass.m_SelfHandle);
							}
						}
					}

					CORI_CORE_ASSERT(m_PassRegistry->m_SortedPassOrderSize == m_PassRegistry->m_PassesSize, "Error when compiling the render graph, graph has a cycle.")
				}

				{
					CORI_PROFILE_SCOPE("Resoruce creation and aqusition.");
					for (auto& node : std::ranges::subrange(m_ResourceRegistry->m_Nodes.begin(), m_ResourceRegistry->m_Nodes.begin() + m_ResourceRegistry->m_NodesSize)) {
						if (node.origin == ResourceOrigin::Created && !node.resourceAllocated) {
							switch (node.type) {
							case ResourceType::Image:
								{
									auto [it, isNew] = m_ResourceRegistry->m_ImagePool.try_emplace(PooledImageDescription(std::get<ImageCreateInfo>(node.createInfo)));
									if (isNew) {
										VulkanImage::CreateInfo createInfo {
											.imageCreateInfo = &std::get<ImageCreateInfo>(node.createInfo).imageCreateInfo,
											.allocationCreateInfo = &std::get<ImageCreateInfo>(node.createInfo).allocationCreateInfo,
											.name = std::get<ImageCreateInfo>(node.createInfo).name,
										};

										auto image = VulkanImage::Create(createInfo);
										it->second.image = image;
										node.resource.emplace<VulkanImage>(image);

										node.resourceAllocated = true;
									} else {
										node.resource.emplace<VulkanImage>(it->second.image);
										it->second.framesAgoUsed = 0;
									}

									break;
								}
							case ResourceType::Buffer:
								{
									auto [it, isNew] = m_ResourceRegistry->m_BufferPool.try_emplace(PooledBufferDescription(std::get<BufferCreateInfo>(node.createInfo)));
									if (isNew) {
										VulkanBuffer::CreateInfo createInfo {
											.bufferCreateInfo = &std::get<BufferCreateInfo>(node.createInfo).bufferCreateInfo,
											.allocationCreateInfo = &std::get<BufferCreateInfo>(node.createInfo).allocationCreateInfo,
											.name = std::get<BufferCreateInfo>(node.createInfo).name,
										};

										auto buffer = VulkanBuffer::Create(createInfo);
										it->second.buffer = buffer;
										node.resource.emplace<VulkanBuffer>(buffer);

										node.resourceAllocated = true;
									} else {
										node.resource.emplace<VulkanBuffer>(it->second.buffer);
										it->second.framesAgoUsed = 0;
									}

									break;
								}
							}
						}
					}
				}

				{
					CORI_PROFILE_SCOPE("Barrier geneartion");
					for (auto& passHandle : std::ranges::subrange(m_PassRegistry->m_SortedPassOrder.begin(), m_PassRegistry->m_SortedPassOrder.begin() + m_PassRegistry->m_SortedPassOrderSize)) {
						Pass& pass = m_PassRegistry->GetPass(passHandle);

						auto ProcessResource = [&](const PassResourceDependency& dep) {
							ResourceNode& node = m_ResourceRegistry->GetNode(dep.resource);

							if (node.resourceAllocated) {
								if (node.type == ResourceType::Image) {
									auto opt = VulkanResourceTracker::TransitionImage(std::get<VulkanImage>(node.resource), std::get<vk::ImageSubresourceRange>(dep.usage.subrange), dep.usage.desiredState);
									if (opt) {
										for (auto& barrier : *opt.value()) {
											pass.AddImageBarrier(std::move(barrier));
										}
									}
								}
								else {
									auto opt = VulkanResourceTracker::TransitionBuffer(std::get<VulkanBuffer>(node.resource), std::get<BufferRange>(dep.usage.subrange).offset, std::get<BufferRange>(dep.usage.subrange).size, dep.usage.desiredState);
									if (opt) {
										for (auto& barrier : *opt.value()) {
											pass.AddBufferBarrier(std::move(barrier));
										}
									}
								}
							}
						};

						for (const auto& write : std::ranges::subrange(pass.m_Writes.begin(), pass.m_Writes.begin() + pass.m_WritesSize)) {
							ProcessResource(write);
						}

						for (const auto& read : std::ranges::subrange(pass.m_Reads.begin(), pass.m_Reads.begin() + pass.m_ReadsSize)) {
							ProcessResource(read);
						}

						m_PassRegistry->m_ResourceInPassSetSize = 0;
					}
				}
			}

			void Execute(vk::CommandBuffer cmb) {
				for (const auto& passHandle : std::ranges::subrange(m_PassRegistry->m_SortedPassOrder.begin(), m_PassRegistry->m_SortedPassOrder.begin() + m_PassRegistry->m_SortedPassOrderSize)) {
					const Pass& pass = m_PassRegistry->GetPass(passHandle);

					if (m_ResourceRegistry->m_SwapChainImageRetrieved) {
						break;
					}

					//CORI_PROFILE_SCOPE(pass.m_Name);

					if (pass.m_ImageBarriersSize > 0 || pass.m_BufferBarriersSize > 0) {
						vk::DependencyInfo depInfo {
							.bufferMemoryBarrierCount = pass.m_BufferBarriersSize,
							.pBufferMemoryBarriers = pass.m_BufferBarriers.data(),
							.imageMemoryBarrierCount = pass.m_ImageBarriersSize,
							.pImageMemoryBarriers = pass.m_ImageBarriers.data()
						};

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
