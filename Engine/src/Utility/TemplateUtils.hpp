#pragma once

namespace Cori {
	namespace Utility {
		/**
		 * @brief Checks if T is present in Types pack.
		 */
		template <typename T, typename... Types>
		concept IsInPack = (std::same_as<T, Types> || ...);

		namespace Internal {
			template <typename... Types>
			inline constexpr bool HasDuplicatesImpl = false;

			template <typename T, typename... Rest>
			inline constexpr bool HasDuplicatesImpl<T, Rest...> = IsInPack<T, Rest...> || HasDuplicatesImpl<Rest...>;
		}

		/**
		 * @brief Checks if Types pack has duplicated types.
		 */
		template <typename... Types>
		concept HasDuplicates = Internal::HasDuplicatesImpl<Types...>;

		/**
		 * @brief Checks if T can be streamed.
		 */
		template <typename T>
		concept IsStreamable = requires(std::ostream& os, const T& val) {
			{ os << val } -> std::same_as<std::ostream&>;
		};

		/**
		 * @brief Checks if T is not bool.
		 */
		template <typename T>
		concept NotBool = !std::is_same_v<std::remove_cvref_t<T>, bool>;
	}
}
