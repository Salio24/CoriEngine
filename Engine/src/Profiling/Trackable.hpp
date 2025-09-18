#pragma once
#include "InstanceMetrics.hpp"

namespace Cori {
	/**
	 * @brief Profiling tools are in this namespace.
	 */
	namespace Profiling {
		/**
		 * @brief For InstanceMetrics to work with a type it should derive from this.
		 * @tparam DerivedType Class type you're deriving Trackable for. Example: class Example : public Trackable<Example>
		 * @tparam BasePack Optional parameter, the types for whom DerivedType will be considered a derived class when using InstanceMetrics. Can be several types.
		 * @details Some examples:
		 * \n I have a class called OpenGLTexture2D, and it is defined something like this:
		 * \n |--|class OpenGLTexture2D final : public Texture2D, public Profiling::Trackable<OpenGLTexture2D, Texture2D, Texture>
		 * \n This way it will be considered derived for Texture2D and Texture in InstanceMetrics.
		 * \n And calling:
		 * \n |--|Profiling::InstanceMetrics<Graphics::Texture>::Report();
		 * \n Will report you data about OpenGLTexture2D, and also calling:
		 * \n |--|Profiling::InstanceMetrics<Graphics::Texture2D>::Report();
		 * \n Will report you data about OpenGLTexture2D.
		 * \n Also when defining a class like this:
		 * \n |--|class SpriteAtlas : public Profiling::Trackable<SpriteAtlas>
		 * \n Calling:
		 * \n |--|Profiling::InstanceMetrics<Graphics::SpriteAtlas>::Report();
		 * \n Will report the data about SpriteAtlas instances.
		 */
		template<typename DerivedType, typename... BasePack>
		class Trackable {
		public:
			Trackable(const Trackable&) = delete;
			Trackable& operator=(const Trackable&) = delete;
			Trackable(Trackable&&) = delete;
			Trackable& operator=(Trackable&&) = delete;

		protected:
			Trackable() {
				InstanceMetrics<DerivedType>::Increment();
				static ReporterRegistrar registrarTrigger;
			}

			~Trackable() {
				InstanceMetrics<DerivedType>::Decrement();
			}

		private:
			struct ReporterRegistrar {
				ReporterRegistrar() {
					if constexpr (sizeof...(BasePack) == 0) {
						Register<DerivedType>();
					} else {
						(Register<BasePack>(), ...);
					}
				}

				template<typename Base>
				void Register() {
					if constexpr (!std::is_same_v<DerivedType, Base>) {
						InstanceMetrics<Base>::RegisterDerivedReporter(
							std::type_index(typeid(DerivedType)),[]() {
								InstanceMetrics<DerivedType>::Report("    ");
							}
						);
						InstanceMetrics<Base>::RegisterDerivedMetricsProvider(
							std::type_index(typeid(DerivedType)), []() -> std::pair<int64_t, int64_t> {
								return { InstanceMetrics<DerivedType>::GetDirectAliveCount(), InstanceMetrics<DerivedType>::GetDirectTotalCreatedCount() };
							}
						);
					}
				}
			};
		};
	}
}