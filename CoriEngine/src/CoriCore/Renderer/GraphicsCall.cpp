// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GraphicsCall.hpp"
#include "Renderer2D.hpp"

namespace Cori {
	CoriGraphicsAPI* GraphicsCall::s_GraphicsAPI = nullptr;
	// make it a namespace interface for corigraphicsAPI
	// this renderer is absolutely uselses as a renderer, it does nothing
	void GraphicsCall::InitRenderers() {
		if CORI_CORE_ASSERT_ERROR(s_GraphicsAPI == nullptr , "Trying reinitialize an already initialized renderer") return;

		s_GraphicsAPI = CoriGraphicsAPI::Create();
		s_GraphicsAPI->Init();

		Renderer2D::Init();
	}

	void GraphicsCall::ShutdownRenderers() {
		delete s_GraphicsAPI;
	}

}