#pragma once

namespace Cori {
	namespace Graphics {
		/**
		 * @brief Converts the RGBA in a hex format to a normalized vec4.
		 * @param hex Value to convert.
		 * @return Normalized vec4 with the color data.
		 */
		consteval glm::vec4 NormalizeHexColor32(const uint32_t hex) {
			glm::vec4 color;
			color.r = (hex >> 24 & 0xFF) / 255.0f;
			color.g = (hex >> 16 & 0xFF) / 255.0f;
			color.b = (hex >> 8  & 0xFF) / 255.0f;
			color.a = (hex & 0xFF) / 255.0f;
			return color;
		}

		/**
		 * @brief Converts the RGB in a hex format to a normalized vec3. Last byte corresponding to Alpha chanel is ignored.
		 * @param hex Value to convert.
		 * @return Normalized vec3 with the color data.
		 */
		consteval glm::vec3 NormalizeHexColor24(const uint32_t hex) {
			glm::vec3 color;
			color.r = (hex >> 24 & 0xFF) / 255.0f;
			color.g = (hex >> 16 & 0xFF) / 255.0f;
			color.b = (hex >> 8  & 0xFF) / 255.0f;
			return color;
		}
	}
}