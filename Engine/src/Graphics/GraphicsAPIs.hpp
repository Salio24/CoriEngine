#pragma once

namespace Cori {
	namespace Graphics {
		enum class GraphicsAPIs {
			/**
			 * @brief Invalid enumerator.
			 */
			None = 0,
			/**
			 * @brief The only one available for now.
			 */
			OpenGL = 1,
			/**
			 * @brief I want to support vulkan, BUT later-later.
			 */
			Vulkan = 2
		};

		[[maybe_unused]] [[nodiscard]] static const char* APIEnumToName(const GraphicsAPIs api) {
			switch (api) {
			case GraphicsAPIs::None:
				return "None";
				break;
			case GraphicsAPIs::OpenGL:
				return "OpenGL";
				break;
			case GraphicsAPIs::Vulkan:
				return "Vulkan";
				break;
			}
			return "";
		}
	}
}
