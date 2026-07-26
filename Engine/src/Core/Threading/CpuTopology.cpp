#include "CpuTopology.hpp"
#include <hwloc.h>
#include "oneapi/tbb/info.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

namespace Cori {
	namespace Threading {
		namespace {
			constexpr std::array<hwloc_obj_type_t, 2> s_CacheLevels{ HWLOC_OBJ_L3CACHE, HWLOC_OBJ_L2CACHE };

			struct Domain {
				hwloc_bitmap_t cpuset{ nullptr };
				uint32_t cpuCount{ 0 };
				uint32_t bestKindIndex{ 0 };
			};

			struct State {
				hwloc_topology_t topology{ nullptr };
				hwloc_bitmap_t allowedCpus{ nullptr };
				std::vector<Domain> domains;
				bool loaded{ false };
				bool hybrid{ false };
			};

			State& Get() {
				static State state;
				return state;
			}
		}

		void CpuTopology::Init() {
			CORI_PROFILE_FUNCTION();

			auto& state = Get();
			if (state.loaded) {
				return;
			}

			if (hwloc_topology_init(&state.topology) != 0) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self }, "hwloc_topology_init failed, CPU placement will be disabled.");
				state.topology = nullptr;
				return;
			}

			if (hwloc_topology_load(state.topology) != 0) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self }, "hwloc_topology_load failed, CPU placement will be disabled.");
				hwloc_topology_destroy(state.topology);
				state.topology = nullptr;
				return;
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "hwloc: {} L3 domain(s), {} PUs | TBB: {} NUMA node(s), {} core type(s)", hwloc_get_nbobjs_by_type(state.topology, HWLOC_OBJ_L3CACHE), hwloc_get_nbobjs_by_type(state.topology, HWLOC_OBJ_PU), tbb::info::numa_nodes().size(), tbb::info::core_types().size());

			state.allowedCpus = hwloc_bitmap_alloc();
			if (hwloc_get_cpubind(state.topology, state.allowedCpus, HWLOC_CPUBIND_PROCESS) != 0 ||
				hwloc_bitmap_iszero(state.allowedCpus)) {
				hwloc_bitmap_copy(state.allowedCpus, hwloc_topology_get_topology_cpuset(state.topology));
			}

			const int kindCount = hwloc_cpukinds_get_nr(state.topology, 0);
			state.hybrid = kindCount > 1;

			for (const auto level : s_CacheLevels) {
				const int count = hwloc_get_nbobjs_by_type(state.topology, level);
				if (count <= 0) {
					continue;
				}

				for (int i = 0; i < count; i++) {
					const hwloc_obj_t obj = hwloc_get_obj_by_type(state.topology, level, i);
					if (!obj || !obj->cpuset) {
						continue;
					}

					Domain domain;
					domain.cpuset = hwloc_bitmap_alloc();
					hwloc_bitmap_and(domain.cpuset, obj->cpuset, state.allowedCpus);
					domain.cpuCount = static_cast<uint32_t>(hwloc_bitmap_weight(domain.cpuset));

					if (domain.cpuCount == 0) {
						hwloc_bitmap_free(domain.cpuset);
						continue;
					}

					if (state.hybrid) {
						for (int kind = 0; kind < kindCount; kind++) {
							hwloc_bitmap_t kindSet = hwloc_bitmap_alloc();
							if (hwloc_cpukinds_get_info(state.topology, static_cast<unsigned>(kind), kindSet, nullptr, nullptr, nullptr, 0) == 0 &&
								hwloc_bitmap_intersects(domain.cpuset, kindSet)) {
								domain.bestKindIndex = static_cast<uint32_t>(kind);
							}
							hwloc_bitmap_free(kindSet);
						}
					}

					state.domains.emplace_back(domain);
				}

				break;
			}

			state.loaded = true;
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "{}", DescribeTopology());
		}

		void CpuTopology::Shutdown() {
			auto& state = Get();

			for (auto& domain : state.domains) {
				hwloc_bitmap_free(domain.cpuset);
			}
			state.domains.clear();

			if (state.allowedCpus) {
				hwloc_bitmap_free(state.allowedCpus);
				state.allowedCpus = nullptr;
			}

			if (state.topology) {
				hwloc_topology_destroy(state.topology);
				state.topology = nullptr;
			}

			state.loaded = false;
			state.hybrid = false;
		}

		uint32_t CpuTopology::LlcDomainCount() {
			return static_cast<uint32_t>(Get().domains.size());
		}

		uint32_t CpuTopology::LlcDomainCpuCount(const uint32_t domain) {
			const auto& domains = Get().domains;
			return domain < domains.size() ? domains[domain].cpuCount : 0;
		}

		std::string CpuTopology::LlcDomainCpuList(const uint32_t domain) {
			const auto& domains = Get().domains;
			if (domain >= domains.size()) {
				return {};
			}

			const int length = hwloc_bitmap_list_snprintf(nullptr, 0, domains[domain].cpuset);
			if (length <= 0) {
				return {};
			}

			std::string out(static_cast<size_t>(length), '\0');
			hwloc_bitmap_list_snprintf(out.data(), out.size() + 1, domains[domain].cpuset);
			return out;
		}

		bool CpuTopology::ShouldBind() {
			const auto& state = Get();
			return state.loaded && state.domains.size() > 1;
		}

		uint32_t CpuTopology::PreferredDomain() {
			const auto& domains = Get().domains;
			if (domains.empty()) {
				return 0;
			}

			uint32_t best = 0;
			for (uint32_t i = 1; i < domains.size(); i++) {
				const bool better = domains[i].bestKindIndex > domains[best].bestKindIndex || (domains[i].bestKindIndex == domains[best].bestKindIndex && domains[i].cpuCount > domains[best].cpuCount);

				if (better) {
					best = i;
				}
			}

			return best;
		}

		bool CpuTopology::BindCurrentThreadToDomain(const uint32_t domain) {
			CORI_PROFILE_FUNCTION();

			auto& state = Get();
			if (!state.loaded || domain >= state.domains.size()) {
				return false;
			}

			if (!ShouldBind()) {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self }, "CPU placement skipped: {} LLC domain(s), nothing to gain from binding.", state.domains.size());
				return false;
			}

			if (hwloc_set_cpubind(state.topology, state.domains[domain].cpuset, HWLOC_CPUBIND_THREAD) != 0) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self }, "Failed to bind thread to LLC domain {} (cpus {}), leaving placement to the scheduler.", domain, LlcDomainCpuList(domain));
				return false;
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Bound thread to LLC domain {} of {} (cpus {}).", domain, state.domains.size(), LlcDomainCpuList(domain));
			return true;
		}

		uint32_t CpuTopology::CurrentCpu() {
			#if defined(_WIN32)
			PROCESSOR_NUMBER processor;
			GetCurrentProcessorNumberEx(&processor);
			return static_cast<uint32_t>(processor.Group) * 64u + static_cast<uint32_t>(processor.Number);
			#elif defined(__linux__)
			const int cpu = sched_getcpu();
			return cpu >= 0 ? static_cast<uint32_t>(cpu) : UINT32_MAX;
			#else
			return UINT32_MAX;
			#endif
		}

		std::string CpuTopology::DescribeTopology() {
			const auto& state = Get();
			if (!state.loaded) {
				return "CPU topology: unavailable, thread placement disabled.";
			}

			std::string out = std::format("CPU topology: {} LLC domain(s){}", state.domains.size(), state.hybrid ? ", hybrid core kinds present" : "");
			for (uint32_t i = 0; i < state.domains.size(); i++) {
				out += std::format("\n  domain {}: {} cpu(s) [{}]", i, state.domains[i].cpuCount, LlcDomainCpuList(i));
			}

			if (!ShouldBind()) {
				out += "\n  -> single domain, thread placement will be skipped";
			} else {
				out += std::format("\n  -> preferred domain for latency-critical threads: {}", PreferredDomain());
			}

			return out;
		}
	}
}
