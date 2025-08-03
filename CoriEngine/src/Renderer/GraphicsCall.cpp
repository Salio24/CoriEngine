#include "GraphicsCall.hpp"
#include "Renderer/Renderer2D.hpp"

namespace Cori {
	std::unique_ptr<CoriGraphicsAPI> GraphicsCall::s_GraphicsAPI = nullptr;
	void GraphicsCall::InitRenderers() {
		if (CORI_CORE_ASSERT_WARN(s_GraphicsAPI == nullptr, "GraphicsCall: Trying to reinitialize an already initialized renderers")) { return; }

		s_GraphicsAPI = CoriGraphicsAPI::Create();
		s_GraphicsAPI->Init();
		Renderer2D::Init();
	}

	void GraphicsCall::ShutdownRenderers() {
		Renderer2D::Shutdown();
		s_GraphicsAPI.reset();
	}

}