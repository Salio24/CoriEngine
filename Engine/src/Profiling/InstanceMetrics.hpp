#pragma once

namespace Cori {
	namespace Profiling {
		/**
		 * @brief This crap needs a rewrite.
		 */
		template<typename T>
		class InstanceMetrics {
		public:
			template<typename DerivedType, typename BaseType>
			friend class Trackable;

			using DerivedMetricsReporter = std::function<void()>;
			using DerivedMetricsProvider = std::function<std::pair<int64_t, int64_t>()>;

			static int64_t GetDirectAliveCount() {
				return s_AliveCount.load(std::memory_order_relaxed);
			}

			static int64_t GetDirectTotalCreatedCount() {
				return s_TotalCreatedCount.load(std::memory_order_relaxed);
			}

			static int64_t GetAliveCount() {
				if (s_DerivedMetricsProviders.empty()) {
					return GetDirectAliveCount();
				}
				return GetAggregatedAliveCount();
			}

			static int64_t GetTotalCreatedCount() {
				if (s_DerivedMetricsProviders.empty()) {
					return GetDirectTotalCreatedCount();
				}
				return GetAggregatedTotalCreatedCount();
			}

			static int64_t GetAggregatedAliveCount() {
				int64_t totalAlive = GetDirectAliveCount();
				for (const auto& val : s_DerivedMetricsProviders | std::views::values) {
					totalAlive += val().first;
				}
				return totalAlive;
			}

			static int64_t GetAggregatedTotalCreatedCount() {
				int64_t totalCreatedSum = GetDirectTotalCreatedCount();
				for (const auto& val : s_DerivedMetricsProviders | std::views::values) {
					totalCreatedSum += val().second;
				}
				return totalCreatedSum;
			}

			static void RegisterDerivedReporter(const std::type_index& derivedTypeId, DerivedMetricsReporter reporter) {
				if (!s_DerivedMetricsReporters.contains(derivedTypeId)) {
					s_DerivedMetricsReporters.insert({ derivedTypeId, reporter });
					CORI_CORE_INFO_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "<{}>: Registered reporter for derived type: <{}>", CORI_CLEAN_TYPE_NAME(T), CORI_DEMANGLE(derivedTypeId.name()));
				}
			}

			static void Report(const std::string& indent = "") {
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "{}Instance Metrics Report for: <{}>", indent, CORI_CLEAN_TYPE_NAME(T));
				if (GetDirectAliveCount() != 0 || GetDirectTotalCreatedCount() != 0 || s_DerivedMetricsReporters.empty()) {
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "{}| Directly Tracked Instances of: <{}>", indent, CORI_CLEAN_TYPE_NAME(T));
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "{}|   Currently Alive: {}", indent, GetDirectAliveCount());
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "{}|   Total Created: {}", indent, GetDirectTotalCreatedCount());
				}

				if (!s_DerivedMetricsReporters.empty()) {
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "{}  Reporting for Registered Derived Types of: <{}>", indent, CORI_CLEAN_TYPE_NAME(T));
					for (const auto& val : s_DerivedMetricsReporters | std::views::values) {
						val();
					}
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "{}  End of Derived Types Report for: <{}>", indent, CORI_CLEAN_TYPE_NAME(T));
				}
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "{}End of Report for: <{}>", indent, CORI_CLEAN_TYPE_NAME(T));
			}

			static void RegisterDerivedMetricsProvider(const std::type_index& derivedTypeId, DerivedMetricsProvider provider) {
				if (!s_DerivedMetricsProviders.contains(derivedTypeId)) {
					s_DerivedMetricsProviders.insert({ derivedTypeId, provider });
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Profiler::Self, Logger::Tags::Profiler::InstanceMetrics }, "<{}>: Registered metrics provider for derived type: <{}>", CORI_CLEAN_TYPE_NAME(T), CORI_DEMANGLE(derivedTypeId.name()));
				}
			}

			static std::vector<std::pair<std::string, int64_t>> GetAliveCountData() {
				std::vector<std::pair<std::string, int64_t>> counts;

				if (GetDirectAliveCount() > 0 || s_DerivedMetricsProviders.empty()) {
					counts.push_back({ typeid(T).name(), GetDirectAliveCount() });
				}

				for (const auto& [type, func] : s_DerivedMetricsProviders) {
					auto [createdCount, _] = func();
					if (createdCount > 0) {
						counts.push_back({ type.name(), createdCount });
					}
				}
				return counts;
			}

			static std::vector<std::pair<std::string, int64_t>> GetTotalCreatedCountData() {
				std::vector<std::pair<std::string, int64_t>> counts;

				if (GetDirectTotalCreatedCount() > 0 || s_DerivedMetricsProviders.empty()) {
					counts.push_back({ typeid(T).name(), GetDirectTotalCreatedCount() });
				}

				for (const auto& [type,  func] : s_DerivedMetricsProviders) {
					auto [_, aliveCount] =  func();
					if (aliveCount > 0) {
						counts.push_back({ type.name(), aliveCount });
					}
				}
				return counts;
			}

		protected:
			static void Increment() {
				s_AliveCount.fetch_add(1, std::memory_order_relaxed);
				s_TotalCreatedCount.fetch_add(1, std::memory_order_relaxed);
			}

			static void Decrement() {
				s_AliveCount.fetch_sub(1, std::memory_order_relaxed);
			}

		private:

			inline static std::atomic<int64_t> s_AliveCount{ 0 };
			inline static std::atomic<int64_t> s_TotalCreatedCount{ 0 };

			inline static std::map<std::type_index, DerivedMetricsReporter> s_DerivedMetricsReporters;
			inline static std::map<std::type_index, DerivedMetricsProvider> s_DerivedMetricsProviders;
		};

	}

	/*
	InstanceMetric usage example:

	InstanceMetrics<ExampleClass>::Report();
	// this will log all the available instance metrics (Currently Alive, Total Created)

	InstanceMetrics<ExampleClass>::GetAliveInstances();
	// will return the number of currently alive instances of the ExampleClass

	InstanceMetrics<ExampleClass>::GetTotalCreated();
	// will return the total number of created instances of the ExampleClass
	*/
}