#pragma once
#include "Graphics/Texture.hpp"

namespace Cori {
	namespace Graphics {
		struct AnimationFrame {
			UVs m_UVs;
			uint32_t m_TickDuration;
		};
	}
}