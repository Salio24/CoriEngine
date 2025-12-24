#pragma once

namespace Cori {
	namespace Math {
		[[nodiscard]] static uint64_t AlignUp(const uint64_t value, const uint64_t alignment) {
			return (value + alignment - 1) & ~(alignment - 1);
		}
	}
}
