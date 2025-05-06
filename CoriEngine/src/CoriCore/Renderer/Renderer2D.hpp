#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "Camera.hpp"
#include "PipelineProgram.hpp"
#include "Texture.hpp"

namespace Cori {

	struct QuadBatchVertexSetup {
		glm::vec2 Position{ 0.0f };
		glm::vec4 Color{ 0.0f };
	};

	struct TexturedQuadVertexSetup {
		glm::vec2 Position{ 0.0f };
		glm::vec2 TexturePosition{ 0.0f };
	};

	enum class BatchDrawType {
		FLAT_COLOR_QUAD,
		TEXTURED_QUAD,
		DEFAULT
	};

	class Renderer2D {
	public:

		static void Init();

		static void Shutdown();

		static void BeginBatch(const glm::mat4& viewProjection, const glm::mat4& model = glm::mat4(1.0f));

		static void EndBatch();

		static void ResetDebugStats();

		static uint32_t GetDrawCallCount();


		static void DrawQuad(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color);

		static void DrawQuad(const glm::vec2 position, const glm::vec2 size, std::shared_ptr<Texture2D>& texture, const glm::vec2 textureSize = glm::vec2(1.0f), const glm::vec2 texturePosition = glm::vec2(0.0f));
		
	private:

		// vvv FOR DEBUG ONLY
		static void SetQuadsPerDraw(const uint32_t count);
		
		static void NewBatch();

		static void BeginBatch_FlatColorQuad();
		static void BeginBatch_TexturedQuad();

		static void EndBatch_FlatColorQuad();
		static void EndBatch_TexturedQuad();


		// for batch rendering
		// global
		static size_t s_MaxQuadCount;
		static size_t s_MaxVertexCount;
		static size_t s_MaxIndexCount;
		static glm::mat4 s_CurrentBatchModelMatrix;
		static glm::mat4 s_CurrentBatchViewProjectionMatrix;
		static BatchDrawType s_CurrentBatchDrawType;

		static bool s_BatchActive;

		static uint32_t s_DrawCallCount;

		// flat color quad
		static std::shared_ptr<ShaderProgram> s_Shader_FlatColorQuad;
		static std::shared_ptr<VertexArray> s_VertexArray_FlatColorQuad;
		static std::shared_ptr<VertexBuffer> s_VertexBuffer_FlatColorQuad;
		static std::shared_ptr<IndexBuffer> s_IndexBuffer_FlatColorQuad;

		static uint32_t s_IndexCount_FlatColorQuad;
		static QuadBatchVertexSetup* s_VertexDataBuffer_FlatColorQuad;
		static QuadBatchVertexSetup* s_VertexDataBufferPtr_FlatColorQuad;

		// textured quad
		static std::shared_ptr<ShaderProgram> s_Shader_TexturedQuad;
		static std::shared_ptr<VertexArray> s_VertexArray_TexturedQuad;
		static std::shared_ptr<VertexBuffer> s_VertexBuffer_TexturedQuad;
		static std::shared_ptr<IndexBuffer> s_IndexBuffer_TexturedQuad;

		static std::shared_ptr<Texture2D> s_CurrentTexture_TexturedQuad;

		static uint32_t s_IndexCount_TexturedQuad;
		static TexturedQuadVertexSetup* s_VertexDataBuffer_TexturedQuad;
		static TexturedQuadVertexSetup* s_VertexDataBufferPtr_TexturedQuad;

	};
}