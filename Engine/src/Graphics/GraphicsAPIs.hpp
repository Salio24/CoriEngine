#pragma once

namespace Cori {
	namespace Graphics {
		enum class GraphicsAPIs {
			None = 0,
			OpenGL = 1,
			Vulkan = 2 // i want to support vulkan, BUT later-later
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
