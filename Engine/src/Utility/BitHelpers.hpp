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

		[[nodiscard]] constexpr uint64_t GetNextPowerOfTwo(const uint64_t value) {
			if (value == 0) {
				return 1;
			}

			if ((value & (value - 1)) == 0) {
				return value;
			}

			return static_cast<uint64_t>(1) << (64 - __builtin_clzll(value));

		}
	}
}