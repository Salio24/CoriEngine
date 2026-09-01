#pragma once

namespace Cori {
	namespace Utility {
		constexpr ImVec4 Hex24ToImVec4(const uint32_t hex, const float alpha = 1.0f) {
			float r = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
			float g = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
			float b = static_cast<float>(hex & 0xFF) / 255.0f;

			return { r, g, b, alpha };
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

		constexpr ImU32 Fade(const ImU32 color, const float alpha) {
			ImVec4 unpacked = ImGui::ColorConvertU32ToFloat4(color);
			unpacked.w *= alpha;
			return ImGui::ColorConvertFloat4ToU32(unpacked);
		}

		constexpr ImVec4 Fade(ImVec4 color, const float alpha) {
			color.w *= alpha;
			return color;
		}

		constexpr ImU32 Shade(const ImU32 color, const float factor) {
			ImVec4 unpacked = ImGui::ColorConvertU32ToFloat4(color);

			unpacked.x = std::min(unpacked.x * factor, 1.0f);
			unpacked.y = std::min(unpacked.y * factor, 1.0f);
			unpacked.z = std::min(unpacked.z * factor, 1.0f);

			return ImGui::ColorConvertFloat4ToU32(unpacked);
		}

		constexpr ImVec4 WithAlpha(const ImVec4 color, const float alpha) {
			return { color.x, color.y, color.z, alpha };
		}

		constexpr ImU32 WithAlpha(const ImU32 color, const float alpha) {
			ImVec4 unpacked = ImGui::ColorConvertU32ToFloat4(color);
			unpacked.w = alpha;
			return ImGui::ColorConvertFloat4ToU32(unpacked);
		}
	}
}