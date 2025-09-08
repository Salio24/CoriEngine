#pragma once

namespace Cori {
	namespace Utility {
		template <typename T, typename... Types>
		concept IsInPack = (std::same_as<T, Types> || ...);

		namespace Internal {
			template <typename... Types>
			inline constexpr bool HasDuplicatesImpl = false;

			template <typename T, typename... Rest>
			inline constexpr bool HasDuplicatesImpl<T, Rest...> = IsInPack<T, Rest...> || HasDuplicatesImpl<Rest...>;
		}

		template <typename... Types>
		concept HasDuplicates = Internal::HasDuplicatesImpl<Types...>;

		template <typename T>
		concept IsStreamable = requires(std::ostream& os, const T& val) {
			{ os << val } -> std::same_as<std::ostream&>;
		};
	}
}
