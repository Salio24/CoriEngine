#pragma once

namespace Cori {
	namespace World {
		using SystemPriority = uint16_t;
		class System;

		template<typename T, typename... Args>
		concept IsSystem = requires(Args&&... args) {
			{ T::Create(std::forward<Args>(args)...) } -> std::same_as<std::shared_ptr<T>>;
			{ T::Priority } -> std::convertible_to<SystemPriority>;
			requires std::derived_from<T, System>;
		};
	}
}