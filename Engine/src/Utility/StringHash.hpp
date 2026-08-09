#pragma once
#include <entt/entt.hpp>

namespace Cori {
	namespace Utility {
		/**
		 * @brief Computes the 64bit FNV-1a string hash.
		 */
		constexpr std::uint64_t fnv1a64(const char *str, const std::size_t len) {
			std::uint64_t hash = 0xcbf29ce484222325ULL; // offset basis
			for (std::size_t i = 0; i < len; ++i) {
				hash ^= static_cast<unsigned char>(str[i]);
				hash *= 0x100000001b3ULL; // fnv prime
			}
			return hash;
		}

		using StringHash64 = uint64_t;
		using StringHash32 = entt::hashed_string::hash_type;

		constexpr StringHash32 HashString32(const char* str) {
			return entt::hashed_string(str).value();
		}

		constexpr StringHash32 HashString32(const std::string& str) {
			return entt::hashed_string(str.c_str()).value();
		}

		constexpr StringHash64 HashString64(const char* str) {
			return fnv1a64(str, strlen(str));
		}

		constexpr StringHash64 HashString64(const std::string_view str) {
			return fnv1a64(str.data(), str.size());
		}
	}
}

/**
 * @brief A consteval operator to seamlessly create compile time 32bit FNA-1a string hashes. Usage "StringToHash"_hs32.
 */
[[nodiscard]] consteval Cori::Utility::StringHash32 operator""_hs32(const char* str, [[maybe_unused]] size_t len) {
	return entt::hashed_string(str).value();
}

/**
 * @brief A consteval operator to seamlessly create compile time 64bit FNA-1a string hashes. Usage "StringToHash"_hs64.
 */
[[nodiscard]] consteval Cori::Utility::StringHash64 operator""_hs64(const char* str, const size_t len) {
	return Cori::Utility::fnv1a64(str, len);
}
