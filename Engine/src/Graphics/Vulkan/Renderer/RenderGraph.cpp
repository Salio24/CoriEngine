#include "RenderGraph.hpp"

namespace Cori {
	namespace Graphics {
		GraphResourceHandle<UploadBufferTag> RenderGraph::ImportBuffer(const VulkanVirtualBuffer& buffer, const char* name) {
			Internal::ResourceNode node;
			node.name = name;
			node.origin = Internal::ResourceOrigin::Imported;
			node.resourceData.emplace<Internal::VirtualBufferResourceData>(Internal::VirtualBufferResourceData{ {}, buffer, { vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone } });

			return static_cast<GraphResourceHandle<UploadBufferTag>>(m_ResourceRegistry->AddNode(node));
		}

		GraphResourceHandle<ScratchBufferTag> RenderGraph::CreateBuffer(const ScratchBufferCreateInfo& createInfo, const char* name) {
			Internal::ResourceNode node;
			node.name = name;
			node.origin = Internal::ResourceOrigin::Created;
			node.resourceData.emplace<Internal::VirtualBufferResourceData>(Internal::VirtualBufferResourceData{ createInfo, {}, {} });

			return static_cast<GraphResourceHandle<ScratchBufferTag>>(m_ResourceRegistry->AddNode(node));
		}

		GraphResourceHandle<VulkanImage> RenderGraph::CreateImage(const VulkanImage::CreateInfo& createInfo, const char* name) {
			Internal::ResourceNode node;
			node.name = name;
			node.origin = Internal::ResourceOrigin::Created;

			node.resourceData.emplace<Internal::ImageResourceData>(Internal::ImageResourceData{ { *createInfo.imageCreateInfo, *createInfo.allocationCreateInfo, createInfo.name }, {}, m_ResourceRegistry->GetFreeImageState() });

			return static_cast<GraphResourceHandle<VulkanImage>>(m_ResourceRegistry->AddNode(node));
		}

		Pass& RenderGraph::CreatePass(const char* name) {
			Pass& pass = m_PassRegistry->GetPass();
			pass.Reset(name);

			return pass;
		}

		void RenderGraph::Compile(const uint64_t currentFrameIndex, const uint32_t dstFrameInFlight) {
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
									//uint32_t layerOffset = layer * image.m_ArrayLayers;
									uint32_t layerOffset = layer;
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

		void RenderGraph::Execute(vk::CommandBuffer cmb) {
			for (const auto& passHandle : m_PassRegistry->m_SortedPassOrder) {
				const Pass& pass = m_PassRegistry->GetPass(passHandle);

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

		void RenderGraph::PrintGraph() const {
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

		RenderGraphPassRegistry::RenderGraphPassRegistry() {
			m_Passes.resize(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
			m_SortedPassOrder.reserve(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
			m_ResourceInPassSet.reserve(CORI_RENDER_GRAPH_PER_PASS_UNIQUE_RESOURCES_SET_INLINE_SIZE);
			m_ProducersSet.reserve(CORI_RENDER_GRAPH_PASS_REGISTRY_INLINE_SIZE);
		}

		Pass& RenderGraphPassRegistry::GetPass() {
			if (m_PassesSize >= m_Passes.size()) {
				m_Passes.resize(m_Passes.size() * 1.5f);
			}

			Pass& pass = m_Passes[m_PassesSize];
			pass.m_SelfHandle = m_PassesSize;
			m_PassesSize++;

			return pass;
		}

		void RenderGraphPassRegistry::AddResourceToSet(Internal::GraphResourceHandleBase handle) {
			for (auto& resource : m_ResourceInPassSet) {
				if (resource == handle) {
					return;
				}
			}

			m_ResourceInPassSet.emplace_back(handle);
		}

		void RenderGraphPassRegistry::AddPassToSet(Internal::PassHandle handle) {
			for (auto& pass : m_ProducersSet) {
				if (pass == handle) {
					return;
				}
			}

			m_ProducersSet.emplace_back(handle);
		}

		Pass::Pass() {
			m_Writes.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
			m_Reads.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
			m_Consumers.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
			m_ImageBarriers.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
			m_BufferBarriers.reserve(CORI_RENDER_GRAPH_PASS_BUFFERS_INLINE_BUFFER_SIZE);
		}

		void Pass::Reset(const char* name) {
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

		RenderGraphResourceRegistry::RenderGraphResourceRegistry() {
			m_Nodes.resize(CORI_RENDER_GRAPH_RESOURCE_REGISTRY_INLINE_SIZE);
			m_ImageStateCache.resize(64); //TODO: move to define.
		}

		RenderGraphResourceRegistry::~RenderGraphResourceRegistry() {
			for (auto& vector : m_ImagePool | std::ranges::views::values) {
				for (auto& pooledImage : vector) {
					if (pooledImage.image.m_Image) {
						DeletionQueue::PushImage(pooledImage.image);
					}
				}
			}
		}

		void RenderGraphResourceRegistry::Reset(const uint64_t currentFrameIndex) {
			m_Nodes.clear();
			m_ImageStateCacheSize = 0;

			ClearStalePooledImages(currentFrameIndex);
		}

		Internal::ImageStateHandle RenderGraphResourceRegistry::GetFreeImageState() {
			if (m_ImageStateCacheSize >= m_ImageStateCache.size()) {
				m_ImageStateCache.resize(std::max<std::size_t>(m_ImageStateCache.size() * 1.5f, m_ImageStateCacheSize + 1));
			}

			Internal::ImageState& state = m_ImageStateCache[m_ImageStateCacheSize];
			state.Reset({});

			return m_ImageStateCacheSize++;
		}

		VulkanImage& RenderGraphResourceRegistry::GetPooledImage(const ImageCreateInfo& info, const uint64_t currentFrameIndex) {
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

		void RenderGraphResourceRegistry::ClearStalePooledImages(const uint64_t currentFrameIndex) {
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

		std::size_t Internal::PooledImageDescription::Hasher::operator()(const PooledImageDescription& desc) const noexcept {
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

		Internal::PooledImageDescription::PooledImageDescription(const ImageCreateInfo& fullInfo) {
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

	}
}