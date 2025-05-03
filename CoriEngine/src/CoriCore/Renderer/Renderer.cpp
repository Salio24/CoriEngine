// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Renderer.hpp"

namespace Cori {
	CoriGraphicsAPI* Renderer::s_GraphicsAPI = nullptr;
	void Renderer::Init() {
		if CORI_CORE_ASSERT_ERROR(s_GraphicsAPI == nullptr , "Trying reinitialize an already initialized renderer") return;

		s_GraphicsAPI = CoriGraphicsAPI::Create();
		s_GraphicsAPI->Init();
	}

	void Renderer::Shutdown() {
		delete s_GraphicsAPI;
	}

	void Renderer::BegineScene() {

	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Render(const std::shared_ptr<VertexArray>& vertexArray) {
		vertexArray->Bind();
		s_GraphicsAPI->DrawElements(vertexArray);
		vertexArray->Unbind();
	}

}