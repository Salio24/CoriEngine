#pragma once

namespace Cori {
	namespace Utils {
		template<typename T, typename... Types>
		concept OneOf = (std::same_as<T, Types> || ...);
	}
}