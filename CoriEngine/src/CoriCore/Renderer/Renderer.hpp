#pragma once
#include "CoriGraphicsAPI.hpp"

namespace Cori {
	class Renderer {
	public:
		static void Init();

		static void Shutdown();

		static void BegineScene();
		static void EndScene();

		static void Render(const std::shared_ptr<VertexArray>& vertexArray);

		static CoriGraphicsAPI* GetCoriGraphicsAPI() { return s_GraphicsAPI; }

	private:
		static CoriGraphicsAPI* s_GraphicsAPI;
	};
}
