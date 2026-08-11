#pragma once

namespace Cori {
	namespace Graphics {
		inline constexpr uint32_t s_ThumbnailAtlasExtent{ 4096 };

		struct ThumbnailRect {
			uint32_t x{ 0 };
			uint32_t y{ 0 };
			uint32_t size{ 0 };
		};
	}
}
