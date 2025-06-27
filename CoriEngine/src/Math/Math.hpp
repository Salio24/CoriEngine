#pragma once

namespace Cori {
	namespace Math {
		template <typename T>
		int Sign(T val) {
			if constexpr (std::is_unsigned_v<T>) {
				return T(0) < val;
			}
			else {
				return (T(0) < val) - (val < T(0));
			}
		}
	}
}