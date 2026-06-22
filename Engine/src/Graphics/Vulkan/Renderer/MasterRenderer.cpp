#include "MasterRenderer.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<MasterRenderer> MasterRenderer::s_Instance{ nullptr };

		void MasterRenderer::Init() {
			CORI_CORE_ASSERT(!s_Instance, "MasterRenderer is already initialized.");
			s_Instance = std::unique_ptr<MasterRenderer>(new MasterRenderer());
		}

		void MasterRenderer::Shutdown() {
			s_Instance.reset();
		}

		MasterRenderer& MasterRenderer::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling MasterRenderer::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}