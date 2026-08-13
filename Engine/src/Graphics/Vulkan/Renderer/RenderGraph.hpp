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
				explicit PooledImageDescription(const ImageCreateInfo& fullInfo);

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
					std::size_t operator()(const PooledImageDescription& desc) const noexcept;
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

		struct ScratchBufferTag {};
		struct UploadBufferTag {};

		class RenderGraphResourceRegistry {
		public:
			RenderGraphResourceRegistry();

			~RenderGraphResourceRegistry();

			void Reset(const uint64_t currentFrameIndex);

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

			Internal::ImageStateHandle GetFreeImageState();

			VulkanImage& GetPooledImage(const ImageCreateInfo& info, const uint64_t currentFrameIndex);

			void ClearStalePooledImages(const uint64_t currentFrameIndex);

		private:

			std::vector<Internal::ResourceNode> m_Nodes;
			std::vector<Internal::ImageState> m_ImageStateCache;

			std::unordered_map<Internal::PooledImageDescription, std::vector<Internal::PooledImage>, Internal::PooledImageDescription::Hasher> m_ImagePool;

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
			Pass();

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

			void Reset(const char* name);

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
			RenderGraphPassRegistry();

			void Reset() {
				m_PassesSize = 0;
				m_SortedPassOrder.clear();
			}

		protected:
			friend RenderGraph;

			Pass& GetPass(const Internal::PassHandle handle) {
				return m_Passes[handle];
			}

			Pass& GetPass();

			void AddSortedPass(Internal::PassHandle handle) {
				m_SortedPassOrder.emplace_back(handle);
			}

			void AddResourceToSet(Internal::GraphResourceHandleBase handle);

			void AddPassToSet(Internal::PassHandle handle);

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

			GraphResourceHandle<UploadBufferTag> ImportBuffer(const VulkanVirtualBuffer& buffer, const char* name);

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

			GraphResourceHandle<ScratchBufferTag> CreateBuffer(const ScratchBufferCreateInfo& createInfo, const char* name);

			GraphResourceHandle<VulkanImage> CreateImage(const VulkanImage::CreateInfo& createInfo, const char* name);

			Pass& CreatePass(const char* name);

			void Compile(const uint64_t currentFrameIndex, const uint32_t dstFrameInFlight);

			void Execute(vk::CommandBuffer cmb);

			void PrintGraph() const;

		private:
			RenderGraphResourceRegistry* m_ResourceRegistry;
			RenderGraphPassRegistry* m_PassRegistry;
		};
	}
}
