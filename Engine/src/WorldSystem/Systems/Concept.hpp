#pragma once

namespace Cori {
	namespace World {
		using SystemPriority = uint16_t;
		class System;

		template<typename T, typename... Args>
		concept IsSystem = requires(T& a, Args&&... args) {
			{ a.Create(std::forward<Args>(args)...) } -> std::convertible_to<bool>;
			{ T::Priority } -> std::convertible_to<SystemPriority>;
			requires std::derived_from<T, System>;
		};
	}
}