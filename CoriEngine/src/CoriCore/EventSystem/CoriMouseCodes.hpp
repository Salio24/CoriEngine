#pragma once
#include <string_view>
#include <magic_enum/magic_enum.hpp>

namespace Cori {
	typedef enum CoriMouseCode {
        CORI_MOUSEBUTTON_UNKNOWN = 0,
		CORI_MOUSEBUTTON_LEFT = 1,
		CORI_MOUSEBUTTON_MIDDLE = 2,
		CORI_MOUSEBUTTON_RIGHT = 3,
        CORI_MOUSEBUTTON_X1 = 4,
        CORI_MOUSEBUTTON_X2 = 5
	} CoriMouseCode;

    std::string_view CoriGetKeyName(CoriMouseCode code) {
        std::string_view name = magic_enum::enum_name(code);
        constexpr std::string_view prefix = "CORI_";
        if (name.starts_with(prefix)) {
            name.remove_prefix(prefix.size());
        }
        return name;
    }
}