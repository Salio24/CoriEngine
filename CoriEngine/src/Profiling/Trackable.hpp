#pragma once
#include "InstanceMetrics.hpp"

namespace Cori {
	namespace Profiling {
		template <typename DerivedType, typename BaseType = DerivedType>
		class Trackable {
		public:
			Trackable(const Trackable&) = delete;
			Trackable& operator=(const Trackable&) = delete;
			Trackable(Trackable&&) = delete;
			Trackable& operator=(Trackable&&) = delete;

		protected:
			Trackable() {
				InstanceMetrics<DerivedType>::Increment();
				static ReporterRegistrar registrar_trigger;
			}

			~Trackable() {
				InstanceMetrics<DerivedType>::Decrement();
			}

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
							std::type_index(typeid(DerivedType)), []() -> std::pair<int64_t, int64_t> {
								return { InstanceMetrics<DerivedType>::GetDirectAliveCount(), InstanceMetrics<DerivedType>::GetDirectTotalCreatedCount() };
							}
						);
					}
				}
			};
			//inline static ReporterRegistrar s_ReporterRegistrar;
		};
	}


}