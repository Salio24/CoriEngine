#pragma once

namespace Cori {
	namespace Utility {
		template <class T>
		static void HashCombine(uint64_t& seed, const T& v) {
			std::hash<T> hasher;
			seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
	}
}