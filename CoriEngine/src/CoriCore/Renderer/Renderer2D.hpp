#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "Camera.hpp"
#include "PipelineProgram.hpp"
#include "Texture.hpp"

namespace Cori {

	struct QuadBatchVertexSetup {
		glm::vec2 Position;
		glm::vec4 Color;
		glm::vec2 TexCoords;
	};

	class Renderer2D {
	public:

		static void Init();

		static void Shutdown();

		static void ResizeCamera(int left, int right, int bottom, int top);

		static void DrawQuadSeparate(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color, const glm::mat4& modelMatrix = glm::mat4(1.0f));

		static void BeginBatch(const glm::mat4& model = glm::mat4(1.0f));

		static void DrawQuadBatch(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color);

		static void EndBatch();

		static void Flush();

	private:
		static OrtoCamera s_Camera;

		static std::shared_ptr<ShaderProgram> s_Shader;
		static std::shared_ptr<PipelineProgram> s_Pipeline;
		static std::shared_ptr<Texture2D> s_MissingTexture;

		// for separate rendering
		static std::shared_ptr<VertexArray> s_VertexArray_separate;
		static std::shared_ptr<VertexBuffer> s_VertexBuffer_separate;
		static std::shared_ptr<IndexBuffer> s_IndexBuffer_separate;

		// for batch rendering
		static std::shared_ptr<VertexArray> s_VertexArray_batch;
		static std::shared_ptr<VertexBuffer> s_VertexBuffer_batch;
		static std::shared_ptr<IndexBuffer> s_IndexBuffer_batch;

		static const size_t s_MaxQuadCount{ 1000 };
		static const size_t s_MaxVertexCount{ s_MaxQuadCount * 4 };
		static const size_t s_MaxIndexCount{ s_MaxQuadCount * 6 };

		static glm::mat4 s_CurrentBatchModelMatrix;
		static uint32_t s_QuadIndexCount;

		static QuadBatchVertexSetup* s_QuadBuffer;
		static QuadBatchVertexSetup* s_QuadBufferPtr;

	};
}