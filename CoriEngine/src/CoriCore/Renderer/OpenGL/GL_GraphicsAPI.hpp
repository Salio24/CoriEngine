#pragma once
#include "../CoriGraphicsAPI.hpp"

namespace Cori {
	class OpenGLGraphicsAPI : public CoriGraphicsAPI {
	public:

		virtual void Init() override;

		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void ClearFramebuffer() override;
		
		virtual void DrawElements(const std::shared_ptr<VertexArray>& vertexArray) override;
		
	};
}