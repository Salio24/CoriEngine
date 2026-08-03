#include "VulkanPresentTiming.hpp"
#include "VulkanEngine.hpp"

#if defined(__linux__)
	#define CORI_PRESENT_TIMING_HAS_HOST_CLOCK 0
#else
	#define CORI_PRESENT_TIMING_HAS_HOST_CLOCK 0
#endif


namespace Cori {
	namespace Graphics {
		namespace {
			constexpr std::array s_TrackedStages = {
				vk::PresentStageFlagBitsEXT::eQueueOperationsEnd,
				vk::PresentStageFlagBitsEXT::eImageFirstPixelOut
			};

			constexpr uint32_t s_StageCount = s_TrackedStages.size();

			constexpr uint32_t s_MaxReportedStages = 4;

			constexpr uint32_t s_MaxDrainPerPoll = 16;

			constexpr uint32_t s_PendingSlots = 32;

			constexpr uint64_t s_FramesBetweenCalibrations = 60;

			constexpr uint32_t s_DefaultPresentQueueDepth = 2;

			constexpr uint64_t s_PresentWaitTimeout = 100000000;

			constexpr std::array s_HostDomainPreference = {
				vk::TimeDomainKHR::eClockMonotonicRaw,
				vk::TimeDomainKHR::eClockMonotonic
			};

			class RollingWindow {
			public:
				void Push(const uint64_t nanoseconds) {
					m_Samples[m_Cursor] = nanoseconds;
					m_Cursor = (m_Cursor + 1) % s_Window;
					m_Count = std::min<uint32_t>(m_Count + 1, s_Window);
				}

				[[nodiscard]] uint64_t Percentile(const uint32_t percent) const {
					if (m_Count < s_WarmUp) {
						return 0;
					}

					std::array<uint64_t, s_Window> scratch{};
					std::copy_n(m_Samples.begin(), m_Count, scratch.begin());

					const uint32_t index = std::min((m_Count * percent) / 100, m_Count - 1);
					const auto nth = scratch.begin() + index;
					std::nth_element(scratch.begin(), nth, scratch.begin() + m_Count);

					return *nth;
				}

				void Clear() {
					m_Cursor = 0;
					m_Count = 0;
				}

			private:
				static constexpr uint32_t s_Window = 64;
				static constexpr uint32_t s_WarmUp = 16;

				std::array<uint64_t, s_Window> m_Samples{};
				uint32_t m_Cursor{ 0 };
				uint32_t m_Count{ 0 };
			};

			struct Published {
				std::atomic<uint64_t> lastScanoutHost{ 0 };
				std::atomic<uint64_t> refreshDuration{ 0 };
				std::atomic<uint64_t> pipelineEstimate{ 0 };
				std::atomic<uint64_t> presentToPhotons{ 0 };
				std::atomic<uint64_t> missedRefreshes{ 0 };

				void Clear() {
					lastScanoutHost.store(0, std::memory_order_relaxed);
					refreshDuration.store(0, std::memory_order_relaxed);
					pipelineEstimate.store(0, std::memory_order_relaxed);
					presentToPhotons.store(0, std::memory_order_relaxed);
				}
			};

			Published s_Published{};

			struct PendingPresent {
				uint64_t presentId{ 0 };
				uint64_t inputTimeHost{ 0 };
				uint64_t frameStartHost{ 0 };
				uint64_t presentCallTimeHost{ 0 };
				uint64_t throttleBlockNs{ 0 };
				bool valid{ false };
			};

			struct State {
				bool supported{ false };
				bool enabled{ false };

				vk::Device device{ nullptr };
				vk::SwapchainKHR swapChain{ nullptr };

				vk::PresentStageFlagsEXT requestedStages{};
				vk::TimeDomainKHR hostTimeDomain{ vk::TimeDomainKHR::eClockMonotonic };

				vk::TimeDomainKHR reportTimeDomain{ vk::TimeDomainKHR::ePresentStageLocalEXT };
				uint64_t reportTimeDomainId{ 0 };
				bool hasReportTimeDomain{ false };

				uint64_t timeDomainsCounter{ 0 };

				std::array<int64_t, s_StageCount> stageToHostOffsets{};
				bool calibrated{ false };
				uint64_t framesSinceCalibration{ 0 };

				uint32_t timingQueueSize{ 0 };
				uint32_t inFlightRecords{ 0 };

				std::array<PendingPresent, s_PendingSlots> pending{};

				vk::PresentId2KHR presentIdInfo{};
				vk::PresentTimingsInfoEXT timingsInfo{};
				vk::PresentTimingInfoEXT timingInfo{};
				uint64_t presentIdValue{ 0 };

				uint64_t nextPresentId{ 1 };
				PendingPresent staged{};
				bool stagedThisPresent{ false };

				uint64_t sdlEpochHost{ 0 };

				uint64_t droppedForFullQueue{ 0 };

				RollingWindow pipelineWindow{};
				RollingWindow presentToPhotonsWindow{};
				uint64_t refreshDuration{ 0 };
				uint64_t lastScanoutHost{ 0 };

				bool waitSupported{ false };
				uint32_t presentQueueDepth{ s_DefaultPresentQueueDepth };
				uint64_t lastPresentedId{ 0 };
				uint64_t presentsOnCurrentSwapChain{ 0 };
				uint64_t lastThrottleBlockNs{ 0 };
			};

			State s_State{};

			[[nodiscard]] uint64_t NormaliseHostTimestamp([[maybe_unused]] const uint64_t raw) {
				#if CORI_PRESENT_TIMING_HAS_HOST_CLOCK
				return raw;
				#else
				return 0;
				#endif
			}

			[[nodiscard]] uint64_t ReadHostClock() {
				#if !CORI_PRESENT_TIMING_HAS_HOST_CLOCK
				return 0;
				#else
				timespec ts{};
				clock_gettime(s_State.hostTimeDomain == vk::TimeDomainKHR::eClockMonotonicRaw ? CLOCK_MONOTONIC_RAW : CLOCK_MONOTONIC, &ts);
				return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
				#endif
			}

			[[nodiscard]] uint32_t StageIndex(const vk::PresentStageFlagsEXT stage) {
				for (uint32_t i = 0; i < s_StageCount; i++) {
					if (stage == vk::PresentStageFlagsEXT(s_TrackedStages[i])) {
						return i;
					}
				}

				return UINT32_MAX;
			}

			void ResetPending() {
				s_State.pending.fill(PendingPresent{});
				s_State.inFlightRecords = 0;
				s_State.stagedThisPresent = false;
			}

			[[nodiscard]] bool SelectReportTimeDomain() {
				s_State.hasReportTimeDomain = false;

				uint64_t domainCounter = 0;
				vk::SwapchainTimeDomainPropertiesEXT domainProperties{};
				vk::Result result = s_State.device.getSwapchainTimeDomainPropertiesEXT(s_State.swapChain, &domainProperties, &domainCounter);
				s_State.timeDomainsCounter = domainCounter;

				if (result != vk::Result::eSuccess || domainProperties.timeDomainCount == 0) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to enumerate swapchain time domains. Error: {}.", vk::to_string(result));
					return false;
				}

				std::vector<vk::TimeDomainKHR> domains(domainProperties.timeDomainCount);
				std::vector<uint64_t> domainIds(domainProperties.timeDomainCount);
				domainProperties.pTimeDomains = domains.data();
				domainProperties.pTimeDomainIds = domainIds.data();

				result = s_State.device.getSwapchainTimeDomainPropertiesEXT(s_State.swapChain, &domainProperties, &domainCounter);
				if (result != vk::Result::eSuccess) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to read swapchain time domains. Error: {}.", vk::to_string(result));
					return false;
				}

				auto pick = [&](const vk::TimeDomainKHR wanted) {
					for (uint32_t i = 0; i < domainProperties.timeDomainCount; i++) {
						if (domains[i] == wanted) {
							s_State.reportTimeDomain = wanted;
							s_State.reportTimeDomainId = domainIds[i];
							s_State.hasReportTimeDomain = true;
							return true;
						}
					}

					return false;
				};

				if (!pick(s_State.hostTimeDomain) && !pick(vk::TimeDomainKHR::eSwapchainLocalEXT) && !pick(vk::TimeDomainKHR::ePresentStageLocalEXT)) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "The swapchain exposes no usable time domain.");
					return false;
				}

				return true;
			}

			void RefreshTimingProperties() {
				if (!s_State.hasReportTimeDomain) {
					return;
				}

				uint64_t counter = 0;
				vk::SwapchainTimingPropertiesEXT properties{};

				const vk::Result result = s_State.device.getSwapchainTimingPropertiesEXT(s_State.swapChain, &properties, &counter);
				if (result != vk::Result::eSuccess || properties.refreshDuration == 0) {
					return;
				}

				s_State.refreshDuration = properties.refreshDuration;
				s_Published.refreshDuration.store(properties.refreshDuration, std::memory_order_relaxed);
			}
		}

		void VulkanPresentTiming::RequestExtensions() {
			#if !CORI_PRESENT_TIMING_HAS_HOST_CLOCK
			return;
			#else
			VulkanEngine::RequestInstanceExtension(vk::KHRGetSurfaceCapabilities2ExtensionName);
			VulkanEngine::RequestOptionalDeviceExtension(vk::EXTPresentTimingExtensionName);
			VulkanEngine::RequestOptionalDeviceExtension(vk::KHRPresentId2ExtensionName);
			VulkanEngine::RequestOptionalDeviceExtension(vk::KHRCalibratedTimestampsExtensionName);
			VulkanEngine::RequestOptionalDeviceExtension(vk::KHRPresentWait2ExtensionName);
			#endif
		}

		bool VulkanPresentTiming::ResolveSupport(const vk::PhysicalDevice physicalDevice, const vk::SurfaceKHR surface) {
			s_State.supported = false;
			s_State.waitSupported = false;

			if (!VulkanEngine::IsDeviceExtensionEnabled(vk::EXTPresentTimingExtensionName)
				|| !VulkanEngine::IsDeviceExtensionEnabled(vk::KHRPresentId2ExtensionName)
				|| !VulkanEngine::IsDeviceExtensionEnabled(vk::KHRCalibratedTimestampsExtensionName)) {
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Present timing is unavailable, one of VK_EXT_present_timing / VK_KHR_present_id2 / VK_KHR_calibrated_timestamps is not enabled. Input latency will not be measured.");
				return false;
			}

			auto features = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDevicePresentTimingFeaturesEXT, vk::PhysicalDevicePresentId2FeaturesKHR>();

			if (!features.get<vk::PhysicalDevicePresentTimingFeaturesEXT>().presentTiming || !features.get<vk::PhysicalDevicePresentId2FeaturesKHR>().presentId2) {
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Present timing is unavailable, the device does not expose the presentTiming / presentId2 features. Input latency will not be measured.");
				return false;
			}

			vk::SurfaceCapabilitiesPresentWait2KHR waitCapabilities{};
			vk::PresentTimingSurfaceCapabilitiesEXT timingCapabilities{ .pNext = &waitCapabilities };
			vk::SurfaceCapabilities2KHR capabilities{ .pNext = &timingCapabilities };
			const vk::PhysicalDeviceSurfaceInfo2KHR surfaceInfo{ .surface = surface };

			const vk::Result result = physicalDevice.getSurfaceCapabilities2KHR(&surfaceInfo, &capabilities);
			if (result != vk::Result::eSuccess) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to query present timing surface capabilities. Error: {}. Input latency will not be measured.", vk::to_string(result));
				return false;
			}

			if (!timingCapabilities.presentTimingSupported) {
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "The surface does not support present timing, so input latency will not be measured. This is expected on X11 and XWayland - run with SDL_VIDEO_DRIVER=wayland on a Wayland session to get the measurement.");
				return false;
			}

			vk::PresentStageFlagsEXT requested{};
			for (const auto stage : s_TrackedStages) {
				if (timingCapabilities.presentStageQueries & stage) {
					requested |= stage;
				}
			}

			if (!(requested & vk::PresentStageFlagBitsEXT::eImageFirstPixelOut)) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "The surface cannot report the ImageFirstPixelOut present stage, so end to end input latency cannot be measured. Reported stages: {}.", vk::to_string(timingCapabilities.presentStageQueries));
				return false;
			}

			s_State.requestedStages = requested;

			auto [domainResult, hostDomains] = physicalDevice.getCalibrateableTimeDomainsKHR();
			if (domainResult != vk::Result::eSuccess) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to query calibrateable time domains. Error: {}. Input latency will not be measured.", vk::to_string(domainResult));
				return false;
			}

			const auto hostDomain = std::ranges::find_if(s_HostDomainPreference, [&hostDomains](const vk::TimeDomainKHR wanted) {
				return std::ranges::contains(hostDomains, wanted);
			});

			if (hostDomain == s_HostDomainPreference.end()) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "The device exposes no host time domain this platform can read back. Input latency will not be measured.");
				return false;
			}

			s_State.hostTimeDomain = *hostDomain;
			s_State.supported = true;

			s_State.waitSupported = VulkanEngine::IsDeviceExtensionEnabled(vk::KHRPresentWait2ExtensionName)
				&& physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDevicePresentWait2FeaturesKHR>().get<vk::PhysicalDevicePresentWait2FeaturesKHR>().presentWait2
				&& waitCapabilities.presentWait2Supported;

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Present timing is supported. Stages: {}. Host time domain: {}. Present queue throttle: {}.", vk::to_string(requested), vk::to_string(s_State.hostTimeDomain), s_State.waitSupported ? "available" : "unavailable");

			return true;
		}

		bool VulkanPresentTiming::IsEnabled() {
			return s_State.enabled;
		}

		bool VulkanPresentTiming::IsPresentQueueThrottleSupported() {
			return s_State.waitSupported;
		}

		void VulkanPresentTiming::OnDeviceCreated(const vk::Device device) {
			if (!s_State.supported) {
				return;
			}

			s_State.device = device;
			s_State.enabled = true;

			CORI_PROFILER_PLOT_CONFIG("Input latency (ms)", Cori::eNumber, false, true, 0x30D030);
			CORI_PROFILER_PLOT_CONFIG("Input to present call (ms)", Cori::eNumber, false, true, 0xE0A020);
			CORI_PROFILER_PLOT_CONFIG("Present call to photons (ms)", Cori::eNumber, false, true, 0x2080E0);
			CORI_PROFILER_PLOT_CONFIG("GPU done to photons (ms)", Cori::eNumber, false, true, 0x20B0C0);
			CORI_PROFILER_PLOT_CONFIG("Pacer sleep (ms)", Cori::eNumber, false, true, 0x8A7CA0);
			CORI_PROFILER_PLOT_CONFIG("Pacer budget (ms)", Cori::eNumber, false, false, 0x607890);
			CORI_PROFILER_PLOT_CONFIG("Pacer margin (ms)", Cori::eNumber, false, false, 0xC03030);
			CORI_PROFILER_PLOT_CONFIG("Present throttle block (ms)", Cori::eNumber, false, true, 0xE06010);
		}

		vk::SwapchainCreateFlagsKHR VulkanPresentTiming::GetSwapChainCreateFlags() {
			if (!s_State.enabled) {
				return {};
			}

			vk::SwapchainCreateFlagsKHR flags = vk::SwapchainCreateFlagBitsKHR::ePresentTimingEXT | vk::SwapchainCreateFlagBitsKHR::ePresentId2;

			if (s_State.waitSupported) {
				flags |= vk::SwapchainCreateFlagBitsKHR::ePresentWait2;
			}

			return flags;
		}

		void VulkanPresentTiming::OnSwapChainCreated(const vk::SwapchainKHR swapchain, const uint32_t imageCount) {
			if (!s_State.enabled) {
				return;
			}

			s_State.swapChain = swapchain;
			s_State.calibrated = false;
			s_State.framesSinceCalibration = 0;
			s_State.refreshDuration = 0;
			s_State.lastScanoutHost = 0;
			s_State.lastPresentedId = 0;
			s_State.presentsOnCurrentSwapChain = 0;
			s_State.pipelineWindow.Clear();
			s_State.presentToPhotonsWindow.Clear();
			s_Published.Clear();
			ResetPending();

			if (!SelectReportTimeDomain()) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Input latency will not be measured for this swapchain.");
				return;
			}

			s_State.timingQueueSize = std::min<uint32_t>(imageCount * 2, s_PendingSlots - 1);
			const vk::Result result = s_State.device.setSwapchainPresentTimingQueueSizeEXT(swapchain, s_State.timingQueueSize);

			if (result != vk::Result::eSuccess) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to size the present timing queue to {}. Error: {}. Input latency will not be measured for this swapchain.", s_State.timingQueueSize, vk::to_string(result));
				s_State.hasReportTimeDomain = false;
				return;
			}

			Calibrate();
			RefreshTimingProperties();

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Present timing bound to the swapchain. Report domain: {} (id {}), timing queue size: {}, refresh duration: {:.3f} ms.", vk::to_string(s_State.reportTimeDomain), s_State.reportTimeDomainId, s_State.timingQueueSize, static_cast<double>(s_State.refreshDuration) / 1000000.0);
		}

		void VulkanPresentTiming::OnSwapChainDestroyed() {
			if (!s_State.enabled) {
				return;
			}

			s_State.swapChain = nullptr;
			s_State.hasReportTimeDomain = false;
			s_State.lastPresentedId = 0;
			s_State.presentsOnCurrentSwapChain = 0;
			s_State.calibrated = false;
			ResetPending();
		}

		void VulkanPresentTiming::Shutdown() {
			s_State = State{};
			s_Published.Clear();
		}

		void VulkanPresentTiming::Calibrate() {
			if (!s_State.enabled || !s_State.hasReportTimeDomain) {
				return;
			}

			CORI_PROFILE_FUNCTION();

			const uint64_t hostNow = ReadHostClock();
			const uint64_t sdlNow = SDL_GetTicksNS();
			s_State.sdlEpochHost = hostNow - sdlNow;

			if (s_State.reportTimeDomain == s_State.hostTimeDomain) {
				s_State.stageToHostOffsets.fill(0);
				s_State.calibrated = true;
				s_State.framesSinceCalibration = 0;
				return;
			}

			std::array<vk::SwapchainCalibratedTimestampInfoEXT, s_StageCount> swapChainInfos{};
			std::array<vk::CalibratedTimestampInfoKHR, s_StageCount + 1> timestampInfos{};

			uint32_t infoCount = 0;
			std::array<uint32_t, s_StageCount> infoToStage{};

			for (uint32_t i = 0; i < s_StageCount; i++) {
				if (!(s_State.requestedStages & s_TrackedStages[i])) {
					continue;
				}

				swapChainInfos[infoCount] = vk::SwapchainCalibratedTimestampInfoEXT{
					.swapchain = s_State.swapChain,
					.presentStage = s_State.reportTimeDomain == vk::TimeDomainKHR::ePresentStageLocalEXT ? vk::PresentStageFlagsEXT(s_TrackedStages[i]) : vk::PresentStageFlagsEXT{},
					.timeDomainId = s_State.reportTimeDomainId
				};

				timestampInfos[infoCount] = vk::CalibratedTimestampInfoKHR{
					.pNext = &swapChainInfos[infoCount],
					.timeDomain = s_State.reportTimeDomain
				};

				infoToStage[infoCount] = i;
				infoCount++;

				if (s_State.reportTimeDomain != vk::TimeDomainKHR::ePresentStageLocalEXT) {
					break;
				}
			}

			if (infoCount == 0) {
				return;
			}

			timestampInfos[infoCount] = vk::CalibratedTimestampInfoKHR{ .timeDomain = s_State.hostTimeDomain };

			std::array<uint64_t, s_StageCount + 1> timestamps{};
			uint64_t maxDeviation = 0;

			const vk::Result result = s_State.device.getCalibratedTimestampsKHR(infoCount + 1, timestampInfos.data(), timestamps.data(), &maxDeviation);
			if (result != vk::Result::eSuccess) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to calibrate present stage timestamps. Error: {}.", vk::to_string(result));
				return;
			}

			const auto host = static_cast<int64_t>(NormaliseHostTimestamp(timestamps[infoCount]));

			if (s_State.reportTimeDomain == vk::TimeDomainKHR::ePresentStageLocalEXT) {
				for (uint32_t i = 0; i < infoCount; i++) {
					s_State.stageToHostOffsets[infoToStage[i]] = host - static_cast<int64_t>(timestamps[i]);
				}
			} else {
				s_State.stageToHostOffsets.fill(host - static_cast<int64_t>(timestamps[0]));
			}

			s_State.calibrated = true;
			s_State.framesSinceCalibration = 0;
		}

		const void* VulkanPresentTiming::PreparePresent(const FrameLatencyStamps& stamps) {
			s_State.stagedThisPresent = false;

			if (!s_State.enabled || !s_State.hasReportTimeDomain) {
				return nullptr;
			}

			CORI_PROFILE_FUNCTION();

			s_State.presentIdValue = s_State.nextPresentId++;

			s_State.presentIdInfo = vk::PresentId2KHR{
				.swapchainCount = 1,
				.pPresentIds = &s_State.presentIdValue
			};

			if (s_State.inFlightRecords + 1 >= s_State.timingQueueSize) {
				s_State.droppedForFullQueue++;

				if (s_State.droppedForFullQueue == 1 || s_State.droppedForFullQueue % 600 == 0) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "The present timing queue is saturated ({} of {} slots outstanding), skipping the stage query for this present. Total skipped: {}.", s_State.inFlightRecords, s_State.timingQueueSize, s_State.droppedForFullQueue);
				}

				return &s_State.presentIdInfo;
			}

			s_State.timingInfo = vk::PresentTimingInfoEXT{
				.flags = {},
				.targetTime = 0,
				.timeDomainId = s_State.reportTimeDomainId,
				.presentStageQueries = s_State.requestedStages,
				.targetTimeDomainPresentStage = {}
			};

			s_State.timingsInfo = vk::PresentTimingsInfoEXT{
				.swapchainCount = 1,
				.pTimingInfos = &s_State.timingInfo
			};

			s_State.presentIdInfo.pNext = &s_State.timingsInfo;

			s_State.staged = PendingPresent{
				.presentId = s_State.presentIdValue,
				//Zero means the frame consumed no input, the record is still kept so the present
				//side of the breakdown stays continuous.
				.inputTimeHost = stamps.inputTimestampSdl == 0 ? 0 : stamps.inputTimestampSdl + s_State.sdlEpochHost,
				.frameStartHost = stamps.frameStartHost,
				.presentCallTimeHost = ReadHostClock(),
				.throttleBlockNs = s_State.lastThrottleBlockNs,
				.valid = true
			};

			s_State.stagedThisPresent = true;

			return &s_State.presentIdInfo;
		}

		void VulkanPresentTiming::FinishPresent(const vk::Result result) {
			if (!s_State.stagedThisPresent) {
				return;
			}

			s_State.stagedThisPresent = false;

			if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
				return;
			}

			PendingPresent& slot = s_State.pending[s_State.staged.presentId % s_PendingSlots];

			if (slot.valid && s_State.inFlightRecords > 0) {
				s_State.inFlightRecords--;
			}

			slot = s_State.staged;
			s_State.inFlightRecords++;
			s_State.lastPresentedId = s_State.staged.presentId;
			s_State.presentsOnCurrentSwapChain++;
		}

		void VulkanPresentTiming::SetPresentQueueDepth(const uint32_t depth) {
			s_State.presentQueueDepth = depth;
		}

		void VulkanPresentTiming::ThrottlePresentQueue() {
			s_State.lastThrottleBlockNs = 0;

			if (!s_State.enabled || !s_State.waitSupported || s_State.presentQueueDepth == 0 || s_State.swapChain == nullptr) {
				return;
			}

			if (s_State.presentsOnCurrentSwapChain < s_State.presentQueueDepth) {
				return;
			}

			const uint64_t waitFor = s_State.lastPresentedId - (s_State.presentQueueDepth - 1);

			CORI_PROFILE_SCOPE("Present queue throttle");

			const vk::PresentWait2InfoKHR info{
				.presentId = waitFor,
				.timeout = s_PresentWaitTimeout
			};

			const uint64_t blockStart = ReadHostClock();
			const vk::Result result = s_State.device.waitForPresent2KHR(s_State.swapChain, &info);
			const uint64_t blockEnd = ReadHostClock();

			if (blockEnd > blockStart) {
				s_State.lastThrottleBlockNs = blockEnd - blockStart;
			}

			CORI_PROFILER_PLOT("Present throttle block (ms)", static_cast<double>(s_State.lastThrottleBlockNs) / 1000000.0);

			if (result != vk::Result::eSuccess && result != vk::Result::eTimeout && result != vk::Result::eSuboptimalKHR && result != vk::Result::eErrorOutOfDateKHR) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Waiting on present id {} failed. Error: {}.", waitFor, vk::to_string(result));
			}
		}

		void VulkanPresentTiming::Poll() {
			if (!s_State.enabled || !s_State.hasReportTimeDomain) {
				return;
			}

			CORI_PROFILE_FUNCTION();

			if (++s_State.framesSinceCalibration >= s_FramesBetweenCalibrations || !s_State.calibrated) {
				Calibrate();
			}

			if (!s_State.calibrated) {
				return;
			}

			if (s_State.refreshDuration == 0) {
				RefreshTimingProperties();
			}

			const vk::PastPresentationTimingInfoEXT info{
				.flags = {},
				.swapchain = s_State.swapChain
			};

			vk::PastPresentationTimingPropertiesEXT properties{};

			vk::Result result = s_State.device.getPastPresentationTimingEXT(&info, &properties);
			if (result != vk::Result::eSuccess && result != vk::Result::eIncomplete) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to query past presentation timing count. Error: {}.", vk::to_string(result));
				return;
			}

			if (properties.presentationTimingCount == 0) {
				return;
			}

			const uint32_t toDrain = std::min(properties.presentationTimingCount, s_MaxDrainPerPoll);

			std::array<vk::PastPresentationTimingEXT, s_MaxDrainPerPoll> timings{};
			std::array<std::array<vk::PresentStageTimeEXT, s_MaxReportedStages>, s_MaxDrainPerPoll> stageStorage{};

			for (uint32_t i = 0; i < toDrain; i++) {
				timings[i].presentStageCount = s_MaxReportedStages;
				timings[i].pPresentStages = stageStorage[i].data();
			}

			properties.presentationTimingCount = toDrain;
			properties.pPresentationTimings = timings.data();

			result = s_State.device.getPastPresentationTimingEXT(&info, &properties);
			if (result != vk::Result::eSuccess && result != vk::Result::eIncomplete) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "Failed to read past presentation timing. Error: {}.", vk::to_string(result));
				return;
			}

			if (properties.timeDomainsCounter != s_State.timeDomainsCounter) {
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self }, "The swapchain time domains changed, rebinding present timing and dropping the in flight measurements.");

				ResetPending();

				if (SelectReportTimeDomain()) {
					Calibrate();
				}

				return;
			}

			for (uint32_t i = 0; i < properties.presentationTimingCount; i++) {
				const vk::PastPresentationTimingEXT& timing = timings[i];

				PendingPresent& record = s_State.pending[timing.presentId % s_PendingSlots];
				if (!record.valid || record.presentId != timing.presentId) {
					continue;
				}

				record.valid = false;
				if (s_State.inFlightRecords > 0) {
					s_State.inFlightRecords--;
				}

				uint64_t firstPixelOutHost = 0;
				uint64_t queueOperationsEndHost = 0;

				for (uint32_t stage = 0; stage < timing.presentStageCount; stage++) {
					const vk::PresentStageTimeEXT& stageTime = timing.pPresentStages[stage];

					if (stageTime.time == 0) {
						continue;
					}

					const uint32_t index = StageIndex(stageTime.stage);
					if (index == UINT32_MAX) {
						continue;
					}

					const auto host = static_cast<uint64_t>(static_cast<int64_t>(stageTime.time) + s_State.stageToHostOffsets[index]);

					if (stageTime.stage == vk::PresentStageFlagsEXT(vk::PresentStageFlagBitsEXT::eImageFirstPixelOut)) {
						firstPixelOutHost = host;
					} else if (stageTime.stage == vk::PresentStageFlagsEXT(vk::PresentStageFlagBitsEXT::eQueueOperationsEnd)) {
						queueOperationsEndHost = host;
					}
				}

				if (firstPixelOutHost == 0) {
					continue;
				}

				uint64_t frameSpan = record.frameStartHost != 0 && record.presentCallTimeHost > record.frameStartHost
					? record.presentCallTimeHost - record.frameStartHost
					: 0;

				frameSpan -= std::min(frameSpan, record.throttleBlockNs);

				if (s_State.lastScanoutHost != 0 && s_State.refreshDuration != 0 && firstPixelOutHost > s_State.lastScanoutHost
					&& frameSpan != 0 && frameSpan < s_State.refreshDuration) {
					const uint64_t gap = firstPixelOutHost - s_State.lastScanoutHost;
					if (gap > s_State.refreshDuration + s_State.refreshDuration / 2) {
						s_Published.missedRefreshes.fetch_add(1, std::memory_order_relaxed);
					}
				}

				s_State.lastScanoutHost = firstPixelOutHost;
				s_Published.lastScanoutHost.store(firstPixelOutHost, std::memory_order_relaxed);

				if (firstPixelOutHost <= record.presentCallTimeHost) {
					continue;
				}

				const uint64_t presentToPhotons = firstPixelOutHost - record.presentCallTimeHost;
				s_State.presentToPhotonsWindow.Push(presentToPhotons);
				s_Published.presentToPhotons.store(s_State.presentToPhotonsWindow.Percentile(50), std::memory_order_relaxed);

				if (frameSpan != 0) {
					s_State.pipelineWindow.Push(frameSpan);
					s_Published.pipelineEstimate.store(s_State.pipelineWindow.Percentile(95), std::memory_order_relaxed);
				}

				constexpr double toMilliseconds = 1.0 / 1000000.0;

				CORI_PROFILER_PLOT("Present call to photons (ms)", static_cast<double>(presentToPhotons) * toMilliseconds);

				if (queueOperationsEndHost != 0) {
					CORI_PROFILER_PLOT("GPU done to photons (ms)", static_cast<double>(static_cast<int64_t>(firstPixelOutHost) - static_cast<int64_t>(queueOperationsEndHost)) * toMilliseconds);
				}

				if (record.inputTimeHost == 0) {
					continue;
				}

				CORI_PROFILER_PLOT("Input latency (ms)", static_cast<double>(static_cast<int64_t>(firstPixelOutHost) - static_cast<int64_t>(record.inputTimeHost)) * toMilliseconds);
				CORI_PROFILER_PLOT("Input to present call (ms)", static_cast<double>(static_cast<int64_t>(record.presentCallTimeHost) - static_cast<int64_t>(record.inputTimeHost)) * toMilliseconds);
			}
		}

		PacingHints VulkanPresentTiming::GetPacingHints() {
			return PacingHints{
				.lastScanoutHost = s_Published.lastScanoutHost.load(std::memory_order_relaxed),
				.refreshDurationNs = s_Published.refreshDuration.load(std::memory_order_relaxed),
				.pipelineEstimate = s_Published.pipelineEstimate.load(std::memory_order_relaxed),
				.presentToPhotons = s_Published.presentToPhotons.load(std::memory_order_relaxed),
				.missedRefreshes = s_Published.missedRefreshes.load(std::memory_order_relaxed)
			};
		}

		uint64_t VulkanPresentTiming::HostNow() {
			return ReadHostClock();
		}
	}
}
