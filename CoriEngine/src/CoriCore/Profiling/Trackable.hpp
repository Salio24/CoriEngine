#pragma once
#include "InstanceMetrics.hpp"

namespace Cori {

	template <typename DerivedType, typename BaseType = DerivedType>
	class Trackable {
	protected:
		Trackable() {
			InstanceMetrics<DerivedType>::Increment();
			static ReporterRegistrar registrar_trigger;
		}

		~Trackable() {
			InstanceMetrics<DerivedType>::Decrement();
		}

		Trackable(const Trackable&) = delete;
		Trackable& operator=(const Trackable&) = delete;
		Trackable(Trackable&&) = delete;
		Trackable& operator=(Trackable&&) = delete;

	private:
		struct ReporterRegistrar {
			ReporterRegistrar() {
				if constexpr (!std::is_same_v<DerivedType, BaseType>) {
					InstanceMetrics<BaseType>::RegisterDerivedReporter(
						std::type_index(typeid(DerivedType)),[]() {
							InstanceMetrics<DerivedType>::Report("    ");
						}
					);
					InstanceMetrics<BaseType>::RegisterDerivedMetricsProvider(
						std::type_index(typeid(DerivedType)), []() -> std::pair<long, long> {
							return {InstanceMetrics<DerivedType>::GetDirectAliveCount(), InstanceMetrics<DerivedType>::GetDirectTotalCreatedCount()};
						}
					);
				}
			}
		};
		//inline static ReporterRegistrar s_ReporterRegistrar;
	};

}