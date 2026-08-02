#pragma once
#include "Graphics/Vulkan/VulkanPresentTiming.hpp"

namespace Cori {
	namespace Core {
		class FramePacer {
		public:
			uint64_t WaitForFrameStart();

			[[nodiscard]] bool IsPacing() const { return m_Pacing; }

		private:
			static void SleepUntil(uint64_t hostDeadline);

			static constexpr uint64_t s_MarginStepUp{ 500000 };
			static constexpr uint64_t s_MarginStepDown{ 10000 };
			static constexpr uint64_t s_MinMargin{ 250000 };

			static constexpr uint64_t s_SpinTail{ 500000 };

			uint64_t m_MarginNs{ s_MinMargin };
			uint64_t m_LastMissCount{ 0 };
			bool m_Pacing{ false };
		};
	}
}
