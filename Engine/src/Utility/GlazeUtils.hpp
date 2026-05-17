#pragma once
#include <glaze/glaze.hpp>

namespace Cori {
	namespace Utility {

		template<typename T>
		consteval auto VulkanFlagsGlazeHelper(T v) {
			return static_cast<std::underlying_type_t<typename T::BitsType>>(v);
		}

		template <typename T, auto Fallback, glz::string_literal Message = "">
		struct GlazeWithFallback {
			T value{ static_cast<T>(Fallback) };

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

			static constexpr T GetFallback() { return Fallback; }
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
	template <typename T, T Fallback, glz::string_literal Message>
	struct hash<Cori::Utility::GlazeWithFallback<T, Fallback, Message>> {
		size_t operator()(const Cori::Utility::GlazeWithFallback<T, Fallback, Message>& w) const noexcept(noexcept(hash<T>{}(w.value))) {
			return hash<T>{}(w.value);
		}
	};
}

namespace glz {
	template <typename T, T Fallback, string_literal Message>
	struct from<JSON, Cori::Utility::GlazeWithFallback<T, Fallback, Message>> {
		template <auto Opts>
		static void op(Cori::Utility::GlazeWithFallback<T, Fallback, Message>& value, is_context auto&& ctx, auto&& it, auto&& end) {
			T result{ Fallback };
			std::string_view bad_token;
			if constexpr (value.Debug) {
				const auto* token_start = it;
				while (token_start != end && (*token_start == ' ' || *token_start == '\t' || *token_start == '\n' || *token_start == '\r')) {
					++token_start;
				}

				const auto* token_end = token_start;
				while (token_end != end && *token_end != ',' && *token_end != '}' && *token_end != ']') {
					++token_end;
				}

				while (token_end != token_start && (*(token_end-1) == ' ' || *(token_end-1) == '\t' || *(token_end-1) == '\n' || *(token_end-1) == '\r')) {
					--token_end;
				}

				bad_token = {token_start, static_cast<std::size_t>(token_end - token_start)};
			}

			parse<JSON>::op<Opts>(result, ctx, it, end);
			if (static_cast<bool>(ctx.error)) {
				if constexpr (Message.size() != 0 && value.Debug) {
					CORI_CORE_WARN("[GlazeWithFallback] {} | Error: {} | Got: '{}' | Fallback: '{}'", Message.sv(), glz::enum_to_string(ctx.error), bad_token, glz::write<Opts>(Fallback).value_or("?"));
				}

				ctx.error = error_code::none;
				skip_value<JSON>::op<Opts>(ctx, it, end);
				ctx.error = error_code::none;
				value.value = Fallback;
			}
			else {
				value.value = result;
			}
		}
	};

	template <typename T, T Fallback, string_literal Message>
	struct to<JSON, Cori::Utility::GlazeWithFallback<T, Fallback, Message>> {
		template <auto Opts>
		static void op(const Cori::Utility::GlazeWithFallback<T, Fallback, Message>& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
			serialize<JSON>::op<Opts>(value.value, ctx, b, ix);
		}
	};
}