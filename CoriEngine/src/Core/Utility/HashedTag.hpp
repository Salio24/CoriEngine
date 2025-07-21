#pragma once
#include "StringHash.hpp"

#define CORI_DECLARE_TAG(tag) inline constexpr Cori::Utils::StringHash64 tag = #tag##_hs64;
