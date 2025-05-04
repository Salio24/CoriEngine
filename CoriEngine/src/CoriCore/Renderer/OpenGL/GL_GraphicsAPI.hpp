#pragma once
#include "../CoriGraphicsAPI.hpp"

namespace Cori {
	class OpenGLGraphicsAPI : public CoriGraphicsAPI {
	public:

		virtual void Init() override;

		virtual void SetViewport(int x, int y, int width, int height) override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void ClearFramebuffer() override;
		
		virtual void DrawElements(const std::shared_ptr<VertexArray>& vertexArray) override;
		
	};
}