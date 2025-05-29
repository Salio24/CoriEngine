#pragma once

namespace Cori {
	template<typename T>
	struct InstanceMetrics {
		using DerivedMetricsReporter = std::function<void()>;
		using DerivedMetricsProvider = std::function<std::pair<long, long>()>;

		static void Increment() {
			s_AliveCount.fetch_add(1, std::memory_order_relaxed);
			s_TotalCreatedCount.fetch_add(1, std::memory_order_relaxed);
		}

		static void Decrement() { 
			s_AliveCount.fetch_sub(1, std::memory_order_relaxed);
		}

		static long GetDirectAliveCount() {
			return s_AliveCount.load(std::memory_order_relaxed);
		}

		static long GetDirectTotalCreatedCount() {
			return s_TotalCreatedCount.load(std::memory_order_relaxed);
		}

		static long GetAliveCount() {
			if (s_DerivedMetricsProviders.empty()) {
				return GetDirectAliveCount();
			}
			return GetAggregatedAliveCount();
		}

		static long GetTotalCreatedCount() {
			if (s_DerivedMetricsProviders.empty()) {
				return GetDirectTotalCreatedCount();
			}
			return GetAggregatedTotalCreatedCount();
		}

		static long GetAggregatedAliveCount() {
			long total_alive = GetDirectAliveCount();
			for (const auto& pair : s_DerivedMetricsProviders) {
				total_alive += pair.second().first;
			}
			return total_alive;
		}

		static long GetAggregatedTotalCreatedCount() {
			long total_created_sum = GetDirectTotalCreatedCount();
			for (const auto& pair : s_DerivedMetricsProviders) {
				total_created_sum += pair.second().second;
			}
			return total_created_sum;
		}

		static void RegisterDerivedReporter(const std::type_index& derivedTypeId, DerivedMetricsReporter reporter) {
			if (s_DerivedMetricsReporters.find(derivedTypeId) == s_DerivedMetricsReporters.end()) {
				s_DerivedMetricsReporters.insert({ derivedTypeId, reporter });
				CORI_CORE_TRACE("InstanceMetrics<{0}>: Registered reporter for derived type '{1}'", Logger::ColoredText(typeid(T).name(), fmt::color::violet), Logger::ColoredText(derivedTypeId.name(), fmt::color::crimson));
			}
		}

		static void Report(const std::string& indent = "") {
			CORI_CORE_INFO("{0}Instance Metrics Report for: {1}", indent, Logger::ColoredText(typeid(T).name(), fmt::color::violet));
			if (GetDirectAliveCount() != 0 || GetDirectTotalCreatedCount() != 0 || s_DerivedMetricsReporters.empty()) {
				CORI_CORE_DEBUG("{0}| Directly Tracked Instances of: {1}:", indent, Logger::ColoredText(typeid(T).name(), fmt::color::violet));
				CORI_CORE_DEBUG("{0}|   Currently Alive: {1}", indent, GetDirectAliveCount());
				CORI_CORE_DEBUG("{0}|   Total Created: {1}", indent, GetDirectTotalCreatedCount());
			}

			if (!s_DerivedMetricsReporters.empty()) {
				CORI_CORE_DEBUG("{0}  Reporting for Registered Derived Types of: {1}", indent, Logger::ColoredText(typeid(T).name(), fmt::color::crimson));
				for (const auto& derivedReporter : s_DerivedMetricsReporters) {
					derivedReporter.second();
				}
				CORI_CORE_DEBUG("{0}  End of Derived Types Report for: {1}", indent, Logger::ColoredText(typeid(T).name(), fmt::color::crimson));
			}
			CORI_CORE_INFO("{0}End of Report for: {1}: ", indent, Logger::ColoredText(typeid(T).name(), fmt::color::violet));
		}

		static void RegisterDerivedMetricsProvider(const std::type_index& derivedTypeId, DerivedMetricsProvider provider) {
			if (s_DerivedMetricsProviders.find(derivedTypeId) == s_DerivedMetricsProviders.end()) {
				s_DerivedMetricsProviders.insert({ derivedTypeId, provider });
				CORI_CORE_TRACE("InstanceMetrics<{0}>: Registered metrics provider for derived type: '{1}'", Logger::ColoredText(typeid(T).name(), fmt::color::violet), Logger::ColoredText(derivedTypeId.name(), fmt::color::crimson));
			}
		}

		static std::vector<std::pair<std::string, long>> GetAliveCountData() {
			std::vector<std::pair<std::string, long>> counts;

			if (GetDirectAliveCount() > 0 || s_DerivedMetricsProviders.empty()) {
				counts.push_back({ typeid(T).name(), GetDirectAliveCount() });
			}

			for (const auto& pair : s_DerivedMetricsProviders) {
				std::pair<long, long> derived_counts = pair.second();
				if (derived_counts.first > 0) {
					counts.push_back({ pair.first.name(), derived_counts.first });
				}
			}
			return counts;
		}

		static std::vector<std::pair<std::string, long>> GetTotalCreatedCountData() {
			std::vector<std::pair<std::string, long>> counts;

			if (GetDirectTotalCreatedCount() > 0 || s_DerivedMetricsProviders.empty()) {
				counts.push_back({ typeid(T).name(), GetDirectTotalCreatedCount() });
			}

			for (const auto& pair : s_DerivedMetricsProviders) {
				std::pair<long, long> derived_counts = pair.second();
				if (derived_counts.second > 0) {
					counts.push_back({ pair.first.name(), derived_counts.second });
				}
			}
			return counts;
		}

		private:
			inline static std::atomic<long> s_AliveCount{ 0 };
			inline static std::atomic<long> s_TotalCreatedCount{ 0 };

			inline static std::map<std::type_index, DerivedMetricsReporter> s_DerivedMetricsReporters;
			inline static std::map<std::type_index, DerivedMetricsProvider> s_DerivedMetricsProviders;
	};


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