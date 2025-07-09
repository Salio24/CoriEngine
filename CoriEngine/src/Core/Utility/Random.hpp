#pragma once

namespace Cori {
	namespace Utils {
		class RandomUint32 {
		public:
			static uint32_t Gen() {
				return Get().GenImpl();
			}

			static uint32_t Gen(uint32_t min, uint32_t max) {
				return Get().GenImpl(min, max);
			}

		private:
			RandomUint32() : gen(std::random_device{}()) {}
			uint32_t GenImpl() {
				if (!dist_full_range) {
					dist_full_range.emplace();
				}
				return (*dist_full_range)(gen);
			}

			uint32_t GenImpl(uint32_t min, uint32_t max) {
				std::uniform_int_distribution dist(min, max);
				return dist(gen);
			}

			static RandomUint32& Get() {
				static RandomUint32 instance;
				return instance;
			}

			std::mt19937 gen;
			std::optional<std::uniform_int_distribution<uint32_t>> dist_full_range;
		};
	}
}