#pragma once

namespace Cori {
	namespace Utility {
		/**
		 * @brief Random uint32_t generator.
		 */
		class RandomUint32 {
		public:
			/**
			 * @brief Generates a random uint32_t without a range constraints.
			 * @return A random uint32_t.
			 */
			static uint32_t Gen() {
				if (!Get().m_DistFullRange) {
					Get().m_DistFullRange.emplace();
				}
				return (*Get().m_DistFullRange)(Get().gen);
			}

			/**
			 * @brief Generates a random uint32_t with a range constraint.
			 * @param min Minimal viable result.
			 * @param max Maximal viable result.
			 * @return A random uint32_t in range [min, max].
			 */
			static uint32_t Gen(const uint32_t min, const uint32_t max) {
				std::uniform_int_distribution dist(min, max);
				return dist(Get().gen);
			}

		private:
			RandomUint32() : gen(std::random_device{}()) {}

			static RandomUint32& Get() {
				static RandomUint32 instance;
				return instance;
			}

			std::mt19937 gen;
			std::optional<std::uniform_int_distribution<uint32_t>> m_DistFullRange;
		};
	}
}