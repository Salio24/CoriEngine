//legacy from old 2d renderer, need to rewire
#if 0
#include "Animation.hpp"
#include "AnimationPack.hpp"

namespace Cori {
	namespace Graphics {
		glm::u16vec2 Animation::GetFrameSize() const {
			return m_Pack->m_Animations[m_AnimationID].m_FrameSize;
		}

	}
}
#endif