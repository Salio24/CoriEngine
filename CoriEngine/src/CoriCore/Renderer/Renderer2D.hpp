#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "Camera.hpp"
#include "PipelineProgram.hpp"
#include "Renderer.hpp"

namespace Cori {
	class Renderer2D {
	public:

		static void Init();

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	};
}