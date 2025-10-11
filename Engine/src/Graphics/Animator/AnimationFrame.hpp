#pragma once
#include "Graphics/Texture.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			struct AnimationFrame {
				UVs m_UVs;
				uint32_t m_TickDuration;
			};
		}
	}
}