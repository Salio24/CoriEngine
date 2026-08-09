#pragma once

namespace Cori {
	namespace Utility {
		constexpr ImVec4 Hex24ToImVec4(const uint32_t hex) {
			float r = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
			float g = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
			float b = static_cast<float>(hex & 0xFF) / 255.0f;

			return { r, g, b, 1.0f };
		}

		constexpr ImVec4 Hex32ToImVec4(const uint32_t hex) {
			float r = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
			float g = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
			float b = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
			float a = static_cast<float>(hex & 0xFF) / 255.0f;

			return { r, g, b, a };
		}

		constexpr ImU32 Hex32ToImU32(const uint32_t hex) {
			return IM_COL32((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);

		}

		constexpr ImVec4 NormalizeVec4Color(const ImVec4 nonNormal) {
			return { nonNormal.x / 255.0f, nonNormal.y / 255.0f, nonNormal.z / 255.0f, nonNormal.w / 255.0f };
		}
	}
}