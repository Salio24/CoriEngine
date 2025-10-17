#pragma once

namespace Cori {
	/**
	 * @brief Anything custom connected to math is in this namespace.
	 */
	namespace Math {
		/**
		 * @brief Returns a sign of a number.
		 * @param val Input.
		 * @return Returns 1 if input is > 0, 0 if input is 0, and -1 if input is < 0.
		 */
		template <typename T>
		int32_t Sign(T val) {
			if constexpr (std::is_unsigned_v<T>) {
				return T(0) < val;
			}
			else {
				return (T(0) < val) - (val < T(0));
			}
		}
	}
}