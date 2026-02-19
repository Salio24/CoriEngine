#pragma once

namespace Cori {
	namespace Utility {

		template<std::unsigned_integral T, std::unsigned_integral Y>
		constexpr void ValidateIndex(Y index) {
			CORI_CORE_ASSERT(index < sizeof(T) * 8, "Bit index out of range, bit type: {}", CORI_CLEAN_TYPE_NAME(T));
		}

		template<std::unsigned_integral T, std::unsigned_integral Y>
		[[nodiscard]] constexpr bool IsSet(const T value, const Y index) {
			ValidateIndex<T, Y>(index);
			return (value & (T{1} << index)) != 0;
		}

		template<std::unsigned_integral T, std::unsigned_integral Y>
		constexpr void Set(T& value, const Y index) {
			ValidateIndex<T, Y>(index);
			value |= (T{1} << index);
		}

		template<std::unsigned_integral T, std::unsigned_integral Y>
		constexpr void Reset(T& value, const Y index) {
			ValidateIndex<T, Y>(index);
			value &= ~(T{1} << index);
		}

		template<std::unsigned_integral T, std::unsigned_integral Y>
		constexpr void Toggle(T& value, const Y index) {
			ValidateIndex<T, Y>(index);
			value ^= (T{1} << index);
		}
	}
}