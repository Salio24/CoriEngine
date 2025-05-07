// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GraphicsCall.hpp"
#include "Renderer2D.hpp"

namespace Cori {
	std::unique_ptr<CoriGraphicsAPI> GraphicsCall::s_GraphicsAPI = nullptr;
	void GraphicsCall::InitRenderers() {
		if CORI_CORE_ASSERT_ERROR(s_GraphicsAPI == nullptr , "Trying reinitialize an already initialized renderer") return;

		s_GraphicsAPI = CoriGraphicsAPI::Create();
		s_GraphicsAPI->Init();

		Renderer2D::Init();
	}

	void GraphicsCall::ShutdownRenderers() {
		s_GraphicsAPI.reset();
	}

}