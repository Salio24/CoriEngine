#pragma once
#include "StringHash.hpp"
#include <boost/pfr.hpp>

namespace Cori {
	namespace Utility {
		namespace Internal {
			template<typename T>
			consteval std::string_view GetTypeHelper() {
				return __PRETTY_FUNCTION__;
			}

			template<typename T, typename Tuple, std::size_t... Indexes>
			consteval auto GenerateTypeSignature(std::index_sequence<Indexes...>) {
				constexpr size_t finalSize =
					GetTypeHelper<T>().size() + 1 +
					((GetTypeHelper<std::tuple_element_t<Indexes, Tuple>>().size() + 1) + ... ) +
					(sizeof...(Indexes) > 0 ? -1 : 0) + 1 + 1;

				std::array<char, finalSize> signature{};

				char* cursor = signature.data();
				auto append = [&](const std::string_view sv) {
					for (const char c : sv) {
						*cursor++ = c;
					}
				};

				append(GetTypeHelper<T>());
				append("{");
				((append(GetTypeHelper<std::tuple_element_t<Indexes, Tuple>>()), append(Indexes == sizeof...(Indexes) - 1 ? "" : ",")), ...);
				append("}");
				*cursor = '\0';
				return signature;
			}

			template<typename T>
			consteval auto GetTypeSignature() {
				static_assert(std::is_aggregate_v<T>, "Type must be an aggregate.");

				using MemberTuple = decltype(boost::pfr::structure_to_tuple(std::declval<T>()));
				constexpr size_t memberCount = std::tuple_size_v<MemberTuple>;

				return Internal::GenerateTypeSignature<T, MemberTuple>(std::make_index_sequence<memberCount>());
			}
		}

		/**
		 * @brief Generates a stable, unique ID for an aggregate struct at compile-time.
		 * @tparam T Struct type to generate the UID for.
		 * @return Unique struct ID.
		 * @details The ID is sensitive to the struct's name and the order and types of its members.
		 *\n It is stable across different runs for the same compiler.
		 */
		template<typename T>
		consteval uint64_t GetAggregateStructUID() {
			constexpr auto signature = Internal::GetTypeSignature<T>();
			const auto view = std::string_view(signature.data(), signature.size() - 1);
			return fnv1a64(view.data(), view.size());
		}

	}
}
