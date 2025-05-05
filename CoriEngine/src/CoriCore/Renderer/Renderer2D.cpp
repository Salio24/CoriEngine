#include "Renderer2D.hpp"
#include "GraphicsCall.hpp"

namespace Cori {

	std::shared_ptr<Texture2D> Renderer2D::s_MissingTexture;

	std::shared_ptr<VertexArray> Renderer2D::s_VertexArray_separate;
	std::shared_ptr<VertexBuffer> Renderer2D::s_VertexBuffer_separate;
	std::shared_ptr<IndexBuffer> Renderer2D::s_IndexBuffer_separate;

	// for batch rendering
	// global
	glm::mat4 Renderer2D::s_CurrentBatchModelMatrix;
	glm::mat4 Renderer2D::s_CurrentBatchViewProjectionMatrix;
	BatchDrawType Renderer2D::s_CurrentBatchDrawType{ BatchDrawType::DEFAULT };

	// flat color quad
	std::shared_ptr<ShaderProgram> Renderer2D::s_Shader_FlatColorQuad;
	std::shared_ptr<VertexArray> Renderer2D::s_VertexArray_FlatColorQuad;
	std::shared_ptr<VertexBuffer> Renderer2D::s_VertexBuffer_FlatColorQuad;
	std::shared_ptr<IndexBuffer> Renderer2D::s_IndexBuffer_FlatColorQuad;

	uint32_t Renderer2D::s_IndexCount_FlatColorQuad{ 0 };
	QuadBatchVertexSetup* Renderer2D::s_VertexDataBuffer_FlatColorQuad{ nullptr };
	QuadBatchVertexSetup* Renderer2D::s_VertexDataBufferPtr_FlatColorQuad{ nullptr };

	// textured quad
	std::shared_ptr<ShaderProgram> Renderer2D::s_Shader_TexturedQuad;
	std::shared_ptr<VertexArray> Renderer2D::s_VertexArray_TexturedQuad;
	std::shared_ptr<VertexBuffer> Renderer2D::s_VertexBuffer_TexturedQuad;
	std::shared_ptr<IndexBuffer> Renderer2D::s_IndexBuffer_TexturedQuad;

	std::shared_ptr<Texture2D> Renderer2D::s_CurrentTexture_TexturedQuad;

	uint32_t Renderer2D::s_IndexCount_TexturedQuad{ 0 };
	TexturedQuadVertexSetup* Renderer2D::s_VertexDataBuffer_TexturedQuad{ nullptr };
	TexturedQuadVertexSetup* Renderer2D::s_VertexDataBufferPtr_TexturedQuad{ nullptr };



	void Renderer2D::Init() {
		// TODO: add a global path tracker / manager, spitting hardcoded paths all over cpp and hpp files is kind-of a bad idea, want to have all my paths specified in one place

		s_MissingTexture = Texture2D::Create("assets/engine/textures/orientation_test24.png");

		/// need to fix vvv
		//// separate renderer (1 draw call per object)
		//s_VertexArray_separate.reset(VertexArray::Create());
		//s_VertexBuffer_separate.reset(VertexBuffer::Create());
		//s_VertexBuffer_separate->SetLayout({
		//	{ ShaderDataType::Vec2, "a_Position" },
		//	{ ShaderDataType::Vec4, "a_Color" },
		//});
		//
		//s_VertexBuffer_separate->Init(nullptr, 4 * s_VertexBuffer_separate->GetLayout().GetStrinde(), DRAW_TYPE::DYNAMIC);
		//s_VertexArray_separate->AddVertexBuffer(s_VertexBuffer_separate);
		//
		//uint32_t indicesSeparate[6] = { 0, 1, 2, 2, 3, 0 };
		//s_IndexBuffer_separate.reset(IndexBuffer::Create(indicesSeparate, sizeof(indicesSeparate)));
		//s_VertexArray_separate->AddIndexBuffer(s_IndexBuffer_separate);

		// batch renderer
		// global
		uint32_t indicesBatch[s_MaxIndexCount];
		uint32_t offset = 0;
		for (int i = 0; i < s_MaxIndexCount; i += 6) {
			indicesBatch[i + 0] = 0 + offset;
			indicesBatch[i + 1] = 1 + offset;
			indicesBatch[i + 2] = 2 + offset;

			indicesBatch[i + 3] = 2 + offset;
			indicesBatch[i + 4] = 3 + offset;
			indicesBatch[i + 5] = 0 + offset;

			offset += 4;
		}

		// flat color quad
		
		s_Shader_FlatColorQuad.reset(ShaderProgram::Create("assets/engine/shaders/flatColorQuadVert.glsl", "assets/engine/shaders/flatColorQuadFrag.glsl"));

		s_VertexArray_FlatColorQuad.reset(VertexArray::Create());
		s_VertexBuffer_FlatColorQuad.reset(VertexBuffer::Create());
		s_VertexBuffer_FlatColorQuad->SetLayout({
			{ ShaderDataType::Vec2, "a_Position" },
			{ ShaderDataType::Vec4, "a_Color" },
		});

		s_VertexBuffer_FlatColorQuad->Init(nullptr, s_MaxQuadCount * s_VertexBuffer_FlatColorQuad->GetLayout().GetStrinde(), DRAW_TYPE::DYNAMIC);

		s_VertexArray_FlatColorQuad->AddVertexBuffer(s_VertexBuffer_FlatColorQuad);


		s_IndexBuffer_FlatColorQuad.reset(IndexBuffer::Create(indicesBatch, sizeof(indicesBatch)));
		s_VertexArray_FlatColorQuad->AddIndexBuffer(s_IndexBuffer_FlatColorQuad);

		s_VertexDataBuffer_FlatColorQuad = new QuadBatchVertexSetup[s_MaxIndexCount];

		// textured quad

		s_Shader_TexturedQuad.reset(ShaderProgram::Create("assets/engine/shaders/texturedQuadVert.glsl", "assets/engine/shaders/texturedQuadFrag.glsl"));

		s_VertexArray_TexturedQuad.reset(VertexArray::Create());
		s_VertexBuffer_TexturedQuad.reset(VertexBuffer::Create());
		s_VertexBuffer_TexturedQuad->SetLayout({
			{ ShaderDataType::Vec2, "a_Position" },
			{ ShaderDataType::Vec2, "a_TexurePosition" }
		});

		s_VertexBuffer_TexturedQuad->Init(nullptr, s_MaxQuadCount * s_VertexBuffer_TexturedQuad->GetLayout().GetStrinde(), DRAW_TYPE::DYNAMIC);
		s_VertexArray_TexturedQuad->AddVertexBuffer(s_VertexBuffer_TexturedQuad);

		s_IndexBuffer_TexturedQuad.reset(IndexBuffer::Create(indicesBatch, sizeof(indicesBatch)));
		s_VertexArray_TexturedQuad->AddIndexBuffer(s_IndexBuffer_TexturedQuad);

		s_VertexDataBuffer_TexturedQuad = new TexturedQuadVertexSetup[s_MaxIndexCount];
	}

	void Renderer2D::Shutdown() {
		delete[] s_VertexDataBuffer_FlatColorQuad;
	}

	void Renderer2D::DrawQuadSeparate(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color, const glm::mat4& modelMatrix) {
		/// to fix vvvv
		//float vertices[6 * 4] = {
		//	position.x,          position.y,          color.r, color.g, color.b, color.a,
		//	position.x + size.x, position.y,          color.r, color.g, color.b, color.a,
		//	position.x + size.x, position.y + size.y, color.r, color.g, color.b, color.a,
		//	position.x,          position.y + size.y, color.r, color.g, color.b, color.a
		//};
		//
		//s_VertexBuffer_separate->Bind();
		//s_VertexBuffer_separate->SetData(vertices, sizeof(vertices));
		//
		//s_Shader_FlatColorQuad->Bind();
		////s_Shader_FlatColorQuad->SetMat4("u_ViewProjection", s_Camera.GetProjectionMatrix());
		//s_Shader_FlatColorQuad->SetMat4("u_ModelMatrix", modelMatrix);
		//
		//s_VertexArray_separate->Bind();
		////GraphicsCall::DrawElements(s_VertexArray_separate);
		//s_VertexArray_separate->Unbind();
	}

	// batching 

	void Renderer2D::BeginBatch(const glm::mat4& viewProjection, const glm::mat4& model) {

		// maybe separate begin and end batch for each batch renderer, and invoke them from something like scene begin/end?
		// TODO ^^^^^
		s_CurrentBatchModelMatrix = model;
		s_CurrentBatchViewProjectionMatrix = viewProjection;

		BeginBatch_FlatColorQuad();

		BeginBatch_TexturedQuad();
	}

	void Renderer2D::EndBatch() {

		if (s_IndexCount_FlatColorQuad) {
			EndBatch_FlatColorQuad();
		}

		if (s_IndexCount_TexturedQuad) {
			EndBatch_TexturedQuad();
		}
	}

	void Renderer2D::NewBatch() {
		EndBatch();
		BeginBatch(s_CurrentBatchViewProjectionMatrix, s_CurrentBatchModelMatrix);
	}

	// flat color quad

	void Renderer2D::BeginBatch_FlatColorQuad() {
		s_VertexDataBufferPtr_FlatColorQuad = s_VertexDataBuffer_FlatColorQuad;
	}

	void Renderer2D::EndBatch_FlatColorQuad() {
		auto size = reinterpret_cast<uint8_t*>(s_VertexDataBufferPtr_FlatColorQuad) - reinterpret_cast<uint8_t*>(s_VertexDataBuffer_FlatColorQuad);
		s_VertexBuffer_FlatColorQuad->Bind();
		s_VertexBuffer_FlatColorQuad->SetData(s_VertexDataBuffer_FlatColorQuad, size);

		s_Shader_FlatColorQuad->Bind();
		s_Shader_FlatColorQuad->SetMat4("u_ViewProjection", s_CurrentBatchViewProjectionMatrix);
		s_Shader_FlatColorQuad->SetMat4("u_ModelMatrix", s_CurrentBatchModelMatrix);

		s_VertexArray_FlatColorQuad->Bind();
		GraphicsCall::DrawElements(s_VertexArray_FlatColorQuad, s_IndexCount_FlatColorQuad);
		s_IndexCount_FlatColorQuad = 0;
	}

	void Renderer2D::DrawQuad(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color) {
		if (s_IndexCount_FlatColorQuad >= s_MaxIndexCount) {
			NewBatch();
		}

		if (s_CurrentBatchDrawType != BatchDrawType::FLAT_COLOR_QUAD) {
			NewBatch();
			s_CurrentBatchDrawType == BatchDrawType::FLAT_COLOR_QUAD;
		}

		s_VertexDataBufferPtr_FlatColorQuad->Position = { position.x, position.y };
		s_VertexDataBufferPtr_FlatColorQuad->Color = color;
		s_VertexDataBufferPtr_FlatColorQuad++;

		s_VertexDataBufferPtr_FlatColorQuad->Position = { position.x + size.x, position.y };
		s_VertexDataBufferPtr_FlatColorQuad->Color = color;
		s_VertexDataBufferPtr_FlatColorQuad++;

		s_VertexDataBufferPtr_FlatColorQuad->Position = { position.x + size.x, position.y + size.y };
		s_VertexDataBufferPtr_FlatColorQuad->Color = color;
		s_VertexDataBufferPtr_FlatColorQuad++;

		s_VertexDataBufferPtr_FlatColorQuad->Position = { position.x, position.y + size.y };
		s_VertexDataBufferPtr_FlatColorQuad->Color = color;
		s_VertexDataBufferPtr_FlatColorQuad++;

		s_IndexCount_FlatColorQuad += 6;
	}

	// textured quad

	void Renderer2D::BeginBatch_TexturedQuad() {
		s_VertexDataBufferPtr_TexturedQuad = s_VertexDataBuffer_TexturedQuad;
	}

	void Renderer2D::EndBatch_TexturedQuad() {
		auto size = reinterpret_cast<uint8_t*>(s_VertexDataBufferPtr_TexturedQuad) - reinterpret_cast<uint8_t*>(s_VertexDataBuffer_TexturedQuad);
		s_VertexBuffer_TexturedQuad->Bind();
		s_VertexBuffer_TexturedQuad->SetData(s_VertexDataBuffer_TexturedQuad, size);

		s_Shader_TexturedQuad->Bind();
		s_Shader_TexturedQuad->SetMat4("u_ViewProjection", s_CurrentBatchViewProjectionMatrix);
		s_Shader_TexturedQuad->SetMat4("u_ModelMatrix", s_CurrentBatchModelMatrix);
		s_Shader_TexturedQuad->SetInt("u_Texture", 2);

		s_VertexArray_TexturedQuad->Bind();

		s_CurrentTexture_TexturedQuad->Bind(2);
		GraphicsCall::DrawElements(s_VertexArray_TexturedQuad, s_IndexCount_TexturedQuad);
		s_IndexCount_TexturedQuad = 0;
		s_CurrentTexture_TexturedQuad.reset();
	}

	void Renderer2D::DrawQuad(const glm::vec2 position, const glm::vec2 size, std::shared_ptr<Texture2D>& texture, const glm::vec2 textureSize, const glm::vec2 texturePosition) {
		if (s_IndexCount_TexturedQuad >= s_MaxIndexCount) {
			NewBatch();
		}

		if (s_CurrentBatchDrawType != BatchDrawType::TEXTURED_QUAD) {
			NewBatch();
			s_CurrentBatchDrawType == BatchDrawType::TEXTURED_QUAD;
		}

		if (s_CurrentTexture_TexturedQuad != texture) {
			if (s_IndexCount_TexturedQuad == 0) {
				s_CurrentTexture_TexturedQuad = texture;
			}
			else {
				NewBatch();
				s_CurrentTexture_TexturedQuad = texture;
			}
		}

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x, position.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { texturePosition.x, texturePosition.y };
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x + size.x, position.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { texturePosition.x + textureSize.x, texturePosition.y };
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x + size.x, position.y + size.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { texturePosition.x + textureSize.x, texturePosition.y + textureSize.y };
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x, position.y + size.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { texturePosition.x, texturePosition.y + textureSize.y };
		s_VertexDataBufferPtr_TexturedQuad++;

		s_IndexCount_TexturedQuad += 6;

	}

}