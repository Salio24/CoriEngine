#pragma once

namespace Cori {
	namespace Graphics {
		consteval glm::vec4 NormalizeHexColor32(const uint32_t hex) {
			glm::vec4 color;
			color.r = (hex >> 24 & 0xFF) / 255.0f;
			color.g = (hex >> 16 & 0xFF) / 255.0f;
			color.b = (hex >> 8  & 0xFF) / 255.0f;
			color.a = (hex & 0xFF) / 255.0f;
			return color;
		}

		consteval glm::vec3 NormalizeHexColor24(const uint32_t hex) {
			glm::vec3 color;
			color.r = (hex >> 24 & 0xFF) / 255.0f;
			color.g = (hex >> 16 & 0xFF) / 255.0f;
			color.b = (hex >> 8  & 0xFF) / 255.0f;
			return color;
		}
	}
}