#pragma once
#include <glaze/glaze.hpp>

namespace Cori {
	namespace Utility {

		//template<typename T>
		//consteval auto VulkanFlagsGlazeHelper(T v) {
		//	return static_cast<std::underlying_type_t<typename T::BitsType>>(v);
		//}

		template <typename F, typename T>
		concept IsFallbackFactory = requires { { F{}() } -> std::convertible_to<T>; };

		template <typename T, auto Fallback, glz::string_literal Message = "">
		struct GlazeWithFallback {
			static constexpr T MakeFallback() {
				if constexpr (IsFallbackFactory<decltype(Fallback), T>)
					return static_cast<T>(Fallback());
				else
					return static_cast<T>(Fallback);
			}

			T value{ MakeFallback() };

			#ifdef DEBUG_BUILD
			static constexpr bool Debug = true;
			#else
			static constexpr bool Debug = false;
			#endif

			GlazeWithFallback() = default;
			GlazeWithFallback(T v) : value(std::move(v)) {}

			GlazeWithFallback(const GlazeWithFallback&) = default;
			GlazeWithFallback(GlazeWithFallback&&) = default;

			GlazeWithFallback& operator=(const GlazeWithFallback&) = default;
			GlazeWithFallback& operator=(GlazeWithFallback&&) = default;

			GlazeWithFallback& operator=(T v) {
				value = std::move(v);
				return *this;
			}

			operator T() const { return value; }
			operator T&() { return value; }

			T* operator->() { return &value; }
			const T* operator->() const { return &value; }
			T& operator*() { return value; }
			const T& operator*() const { return value; }

			bool operator==(const GlazeWithFallback& o) const { return value == o.value; }
			bool operator!=(const GlazeWithFallback& o) const { return value != o.value; }
			bool operator<(const GlazeWithFallback& o) const { return value < o.value; }
			bool operator<=(const GlazeWithFallback& o) const { return value <= o.value; }
			bool operator>(const GlazeWithFallback& o) const { return value > o.value; }
			bool operator>=(const GlazeWithFallback& o) const { return value >= o.value; }

			bool operator==(const T& o) const { return value == o; }
			bool operator!=(const T& o) const { return value != o; }
			bool operator<(const T& o) const { return value < o; }
			bool operator<=(const T& o) const { return value <= o; }
			bool operator>(const T& o) const { return value > o; }
			bool operator>=(const T& o) const { return value >= o; }

			GlazeWithFallback& operator+=(const T& o) {
				value += o;
				return *this;
			}

			GlazeWithFallback& operator-=(const T& o) {
				value -= o;
				return *this;
			}

			GlazeWithFallback& operator*=(const T& o) {
				value *= o;
				return *this;
			}

			GlazeWithFallback& operator/=(const T& o) {
				value /= o;
				return *this;
			}

			GlazeWithFallback& operator%=(const T& o) {
				value %= o;
				return *this;
			}

			GlazeWithFallback operator+(const T& o) const { return WithFallback(value + o); }
			GlazeWithFallback operator-(const T& o) const { return WithFallback(value - o); }
			GlazeWithFallback operator*(const T& o) const { return WithFallback(value * o); }
			GlazeWithFallback operator/(const T& o) const { return WithFallback(value / o); }
			GlazeWithFallback operator%(const T& o) const { return WithFallback(value % o); }

			GlazeWithFallback operator-() const { return WithFallback(-value); }
			GlazeWithFallback operator+() const { return WithFallback(+value); }

			GlazeWithFallback& operator++() {
				++value;
				return *this;
			}

			GlazeWithFallback operator++(int) {
				auto t = *this;
				++value;
				return t;
			}

			GlazeWithFallback& operator--() {
				--value;
				return *this;
			}

			GlazeWithFallback operator--(int) {
				auto t = *this;
				--value;
				return t;
			}

			GlazeWithFallback& operator&=(const T& o) {
				value &= o;
				return *this;
			}

			GlazeWithFallback& operator|=(const T& o) {
				value |= o;
				return *this;
			}

			GlazeWithFallback& operator^=(const T& o) {
				value ^= o;
				return *this;
			}

			GlazeWithFallback operator&(const T& o) const { return WithFallback(value & o); }
			GlazeWithFallback operator|(const T& o) const { return WithFallback(value | o); }
			GlazeWithFallback operator^(const T& o) const { return WithFallback(value ^ o); }
			GlazeWithFallback operator~() const { return WithFallback(~value); }

			bool operator!() const { return !value; }
			friend std::ostream& operator<<(std::ostream& os, const GlazeWithFallback& w) {
				return os << w.value;
			}

			friend std::istream& operator>>(std::istream& is, GlazeWithFallback& w) {
				return is >> w.value;
			}

			static constexpr T GetFallback() { return MakeFallback(); }
		};

		struct ReflectEnumsOpts : glz::opts {
			bool reflect_enums = true;
			uint32_t format = glz::JSON;
			bool null_terminated = GLZ_NULL_TERMINATED; // Whether the input buffer is null terminated
			bool comments = false; // Support reading in JSONC style comments
			bool error_on_unknown_keys = true; // Error when an unknown key is encountered
			bool skip_null_members = true; // Skip writing out params in an object if the value is null
			bool prettify = false; // Write out prettified JSON
			bool minified = false; // Require minified input for JSON, which results in faster read performance
			bool error_on_missing_keys = false; // Require all non nullable keys to be present in the object. Use

			bool partial_read = false; // Reads into the deepest structural object and then exits without parsing the rest of the input
		};
	}
}

namespace std {
	template <typename T, auto Fallback, glz::string_literal Message>
	struct hash<Cori::Utility::GlazeWithFallback<T, Fallback, Message>> {
		size_t operator()(const Cori::Utility::GlazeWithFallback<T, Fallback, Message>& w) const noexcept(noexcept(hash<T>{}(w.value))) {
			return hash<T>{}(w.value);
		}
	};
}

namespace glz {
	template <typename T, auto Fallback, string_literal Message>
	struct from<JSON, Cori::Utility::GlazeWithFallback<T, Fallback, Message>> {
		template <auto Opts>
		static void op(Cori::Utility::GlazeWithFallback<T, Fallback, Message>& value, is_context auto&& ctx, auto&& it, auto&& end) {
			T result;
			std::string_view badToken;
			if constexpr (value.Debug) {
				const auto* tokenStart = it;
				while (tokenStart != end && (*tokenStart == ' ' || *tokenStart == '\t' || *tokenStart == '\n' || *tokenStart == '\r')) {
					++tokenStart;
				}

				bool arrayOrObject = *tokenStart == '[' || *tokenStart == '{';

				const auto* tokenEnd = tokenStart;

				if (arrayOrObject) {
					while (tokenEnd != end && *(tokenEnd - 1) != '}' && *(tokenEnd - 1) != ']') {
						++tokenEnd;
					}
				} else {
					while (tokenEnd != end && *tokenEnd != ',') {
						++tokenEnd;
					}
				}

				auto c = *(tokenEnd - 1);

				if (!arrayOrObject) {
					while (tokenEnd != tokenStart && (*(tokenEnd - 1) == ' ' || *(tokenEnd - 1) == '\t' || *(tokenEnd - 1) == '\n' || *(tokenEnd - 1) == '\r')) {
						--tokenEnd;
					}
				}


				badToken = {tokenStart, static_cast<uint64_t>(tokenEnd - tokenStart)};
			}

			auto oldIt = it;

			parse<JSON>::op<Opts>(result, ctx, it, end);
			if (static_cast<bool>(ctx.error)) {
				if constexpr (Message.size() != 0 && value.Debug) {
					CORI_CORE_WARN_TAGGED({ Cori::Logger::Tags::Core::Self, Cori::Logger::Tags::Core::Glaze::Self, Cori::Logger::Tags::Core::Glaze::GlazeWithFallback }, "Message: {} | Error: {} | Got: '{}' | Fallback: '{}'", Message.sv(), glz::enum_to_string(ctx.error), badToken, glz::write<Opts>(value.MakeFallback()).value_or("?"));
				}

				ctx.error = error_code::none;
				it = oldIt;
				skip_value<JSON>::op<Opts>(ctx, it, end);
				ctx.error = error_code::none;
				value.value = value.MakeFallback();
			}
			else {
				value.value = result;
			}
		}
	};

	template <typename T, auto Fallback, string_literal Message>
	struct to<JSON, Cori::Utility::GlazeWithFallback<T, Fallback, Message>> {
		template <auto Opts>
		static void op(const Cori::Utility::GlazeWithFallback<T, Fallback, Message>& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
			serialize<JSON>::op<Opts>(value.value, ctx, b, ix);
		}
	};
}