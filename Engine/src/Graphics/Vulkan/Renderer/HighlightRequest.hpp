#pragma once

namespace Cori {
	namespace Graphics {
		struct HighlightRequest {
			uint32_t color{ 0xFFFFFFFF };
			uint32_t renderObjectIndex{ 0 };
		};
	}
}
