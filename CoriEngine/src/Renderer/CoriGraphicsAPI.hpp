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

		virtual void DrawElementsTriangles(const uint32_t elementCount) = 0;

		virtual void DrawElementsInstancedTriangles(const uint32_t instanceCount) = 0;

		virtual void EnableDepthTest() = 0;
		virtual void DisableDepthTest() = 0;

		virtual void EnableBlending() = 0;
		virtual void DisableBlending() = 0;

		virtual void SetDepthMask(bool mode) = 0;

		static std::unique_ptr<CoriGraphicsAPI> Create();
	};
}