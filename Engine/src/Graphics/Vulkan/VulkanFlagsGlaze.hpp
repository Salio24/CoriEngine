#pragma once
#include <meta>
#include "VulkanEngine.hpp"
#include "Utility/GlazeUtils.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtemplate-body"

namespace glz {
	template <typename BitType>
	struct from<JSON, vk::Flags<BitType>> {
		template <auto Opts>
		static void op(vk::Flags<BitType>& flags, is_context auto&& ctx, auto&& it, auto&& end) {
			flags = {};

			glz::match<'['>(ctx, it);
			bool error = false;
			if (static_cast<bool>(ctx.error)) {
				error = true;
			}

			while (true) {
				while (it != end && (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r')) {
					++it;
				}

				if (it == end) {
					ctx.error = error_code::unexpected_end;
					return;
				}

				if (*it == ']') {
					++it;
					break;
				}

				std::string s{};
				parse<JSON>::op<Opts>(s, ctx, it, end);
				if (static_cast<bool>(ctx.error)) {
					error = true;
				}

				if (!error) {
					auto result = glz::string_to_enum<BitType>(s);

					if (!result) {
						ctx.error = error_code::unexpected_enum;
						error = true;
					} else {
						flags |= result.value();
					}
				} else {
					while (it != end && !(*it == ' ' || *it == ']' || *it == '\t' || *it == '\n' || *it == '\r')) {
						++it;
					}
				}

				while (it != end && (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r')) {
					++it;
				}

				if (it != end && *it == ',') {
					++it;
				}
			}
		}
	};

	template <typename BitType>
	struct to<JSON, vk::Flags<BitType>> {
		template <auto Opts>
		static void op(const vk::Flags<BitType>& flags, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
			glz::dump<'['>(b, ix);
			glz::dump<' '>(b, ix);

			bool first = true;

			template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^BitType))) {
				constexpr BitType bit = [:e:];

				if (static_cast<bool>(flags & bit)) {
					if (!first) {
						glz::dump<','>(b, ix);
						glz::dump<' '>(b, ix);
					}
					serialize<JSON>::op<Opts>(std::meta::identifier_of(e), ctx, b, ix);
					first = false;
				}
			}

			glz::dump<' '>(b, ix);
			glz::dump<']'>(b, ix);
		}
	};
}

#pragma GCC diagnostic pop