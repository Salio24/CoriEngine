#include "FramePacer.hpp"

namespace Cori {
	namespace Core {
		uint64_t FramePacer::WaitForFrameStart() {
			CORI_PROFILE_FUNCTION();

			const Graphics::PacingHints hints = Graphics::VulkanPresentTiming::GetPacingHints();
			const uint64_t now = Graphics::VulkanPresentTiming::HostNow();

			if (now == 0 || hints.refreshDurationNs == 0 || hints.lastScanoutHost == 0 || hints.pipelineEstimate == 0 || hints.presentToPhotons == 0) {
				m_Pacing = false;
				return now;
			}

			m_Pacing = true;

			const uint64_t maxMargin = std::max(hints.refreshDurationNs / 2, s_MinMargin);

			if (hints.missedRefreshes != m_LastMissCount) {
				m_LastMissCount = hints.missedRefreshes;
				m_MarginNs = std::min(m_MarginNs + s_MarginStepUp, maxMargin);
			} else if (m_MarginNs > s_MinMargin) {
				m_MarginNs -= std::min(s_MarginStepDown, m_MarginNs - s_MinMargin);
			}

			const uint64_t budget = hints.pipelineEstimate + hints.presentToPhotons + m_MarginNs;

			uint64_t target = hints.lastScanoutHost;
			if (now + budget > target) {
				const uint64_t steps = ((now + budget) - target) / hints.refreshDurationNs + 1;
				target += steps * hints.refreshDurationNs;
			}

			const uint64_t wakeAt = target - budget;

			CORI_PROFILER_PLOT("Pacer margin (ms)", static_cast<double>(m_MarginNs) / 1000000.0);
			CORI_PROFILER_PLOT("Pacer budget (ms)", static_cast<double>(budget) / 1000000.0);
			CORI_PROFILER_PLOT("Pacer sleep (ms)", wakeAt > now ? static_cast<double>(wakeAt - now) / 1000000.0 : 0.0);

			if (wakeAt > now && wakeAt - now < hints.refreshDurationNs) {
				SleepUntil(wakeAt);
			}

			return Graphics::VulkanPresentTiming::HostNow();
		}

		void FramePacer::SleepUntil(const uint64_t hostDeadline) {
			const uint64_t now = Graphics::VulkanPresentTiming::HostNow();

			if (hostDeadline > now + s_SpinTail) {
				std::this_thread::sleep_for(std::chrono::nanoseconds(hostDeadline - now - s_SpinTail));
			}

			while (Graphics::VulkanPresentTiming::HostNow() < hostDeadline) {
				std::this_thread::yield();
			}
		}
	}
}
