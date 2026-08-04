#include "PersistentRenderTarget.hpp"
#include "MasterRenderer.hpp"

namespace Cori {
	namespace Graphics {
		void PersistentRenderTarget::PushImageForInitialTransition() {
			MasterRenderer::PushPRTForInitialTransition(m_Image.m_Image);
		}
	}
}
