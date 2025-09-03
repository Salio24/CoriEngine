#include "API.hpp"
#include "Graphics/Renderer2D.hpp"

namespace Cori {
	std::unique_ptr<CoriGraphicsAPI> API::s_GraphicsAPI = nullptr;
	void API::Init() {
		s_GraphicsAPI = CoriGraphicsAPI::Create();
		s_GraphicsAPI->Init();
		Graphics::Renderer2D::Init();
	}

	void API::Shutdown() {
		Graphics::Renderer2D::Shutdown();
		s_GraphicsAPI.reset();
	}

}