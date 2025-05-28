#pragma once

namespace Cori {
	template<typename T>
	struct InstanceMetrics {
		inline static std::atomic<long> s_aliveInstances{ 0 };
		inline static std::atomic<long> s_totalCreated{ 0 };

		static void Increment() {
			s_aliveInstances++;
			s_totalCreated++;
		}

		static void Decrement() { s_aliveInstances--; }

		static void Report() {
			CORI_CORE_INFO("Reporting Instance Metrics for type: {0}", typeid(T).name());
			CORI_CORE_DEBUG("Currently Alive: {0}", GetAliveInstances());
			CORI_CORE_DEBUG("Total Created: {0}", GetTotalCreated());
			CORI_CORE_INFO("End of the report");
		}

		static long GetAliveInstances() {
			return s_aliveInstances.load();
		}

		static long GetTotalCreated() {
			return s_totalCreated.load();
		}
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