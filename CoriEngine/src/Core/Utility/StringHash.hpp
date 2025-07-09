#pragma once
#include <entt/entt.hpp>

namespace Cori {
	namespace Utils {
		using StringHash = entt::hashed_string::hash_type;

		constexpr StringHash HashString(const char* str) {
			return entt::hashed_string(str).value();
		}

		constexpr StringHash HashString(const std::string& str) {
			return entt::hashed_string(str.c_str()).value();
		}
	}


}

[[nodiscard]] consteval Cori::Utils::StringHash operator""_hs(const char* str, size_t) {
	return entt::hashed_string(str).value();
}
