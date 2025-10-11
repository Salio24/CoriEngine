#pragma once

namespace Cori {
	namespace World {
		using SystemPriority = uint16_t;
		class System;

		template<typename T>
		concept IsSystem = requires(T& a) {
			{ T::Priority } -> std::convertible_to<SystemPriority>;
			requires std::derived_from<T, System>;
		};
	}
}