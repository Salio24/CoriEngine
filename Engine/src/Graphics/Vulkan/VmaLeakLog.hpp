#pragma once

namespace vma {
	class VirtualBlock;
}

namespace Cori {
	namespace Graphics {
		class VmaLeakLog {
		public:
			static void BumpLeakedAllocationCounter();

			static void BumpLeakAssertCounter();

			static std::expected<std::filesystem::path, std::string> WriteLiveAllocationReport(const std::filesystem::path& destination = {});

			static bool DumpLiveAllocations(std::string_view label);

			static bool LogCollectedLeaks(std::string_view label);

		private:
			static std::atomic<uint32_t> s_LeakedAllocations;
			static std::atomic<uint32_t> s_LeakAsserts;
			static std::atomic<uint32_t> s_DumpCounter;
		};
	}
}
