#pragma once

namespace Cori {
	namespace Threading {
		class CpuTopology {
		public:
			static void Init();

			static void Shutdown();

			[[nodiscard]] static uint32_t LlcDomainCount();

			[[nodiscard]] static uint32_t LlcDomainCpuCount(uint32_t domain);

			[[nodiscard]] static std::string LlcDomainCpuList(uint32_t domain);

			[[nodiscard]] static bool ShouldBind();

			[[nodiscard]] static uint32_t PreferredDomain();

			static bool BindCurrentThreadToDomain(uint32_t domain);

			[[nodiscard]] static uint32_t CurrentCpu();

			[[nodiscard]] static std::string DescribeTopology();
		};
	}
}
