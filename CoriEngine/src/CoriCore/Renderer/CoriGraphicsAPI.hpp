#pragma once
#include "VertexArray.hpp"
#include <glm/glm.hpp>

namespace Cori {
	class CoriGraphicsAPI {
	public:
		virtual ~CoriGraphicsAPI() = default;

		virtual void Init() = 0;

		virtual void SetViewport(int x, int y, int width, int height) = 0;

		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void ClearFramebuffer() = 0;

		virtual void DrawElements(const std::shared_ptr<VertexArray>& vertexArray, const uint32_t elementCount) = 0;

		static CoriGraphicsAPI* Create();
	};
}