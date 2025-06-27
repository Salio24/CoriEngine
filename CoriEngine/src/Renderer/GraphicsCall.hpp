#pragma once
#include "CoriGraphicsAPI.hpp"

namespace Cori {
	class GraphicsCall {
	public:
		static void InitRenderers();

		static void ShutdownRenderers();

		static void SetViewport(int x, int y, int width, int height) {
			s_GraphicsAPI->SetViewport(x, y, width, height);
		}

		static void SetClearColor(const glm::vec4& color) {
			s_GraphicsAPI->SetClearColor(color);
		}

		static void ClearFramebuffer() {
			s_GraphicsAPI->ClearFramebuffer();
		}

		static void DrawElements(const std::shared_ptr<VertexArray>& vertexArray, const uint32_t elementCount) {
			s_GraphicsAPI->DrawElements(vertexArray, elementCount);
		}

	private:
		static std::unique_ptr<CoriGraphicsAPI> s_GraphicsAPI;
	};
}
