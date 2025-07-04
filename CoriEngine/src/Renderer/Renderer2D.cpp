// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
// TODO: add a global path tracker / manager, spitting hardcoded paths all over cpp and hpp files is kind-of a bad idea, want to have all my paths specified in one place
#include "Renderer2D.hpp"
#include "GraphicsCall.hpp"
#include "Core/Time.hpp"
#include "Core/Application.hpp"
#include "AssetManager/AssetDefinitions.hpp"

namespace Cori {

	// for batch rendering
	// global

	uint32_t Renderer2D::s_MaxQuadCount{ 3000 };
	uint32_t Renderer2D::s_MaxVertexCount{ s_MaxQuadCount * 4 };
	uint32_t Renderer2D::s_MaxIndexCount{ s_MaxQuadCount * 6 };
	glm::mat4 Renderer2D::s_CurrentBatchModelMatrix;
	glm::mat4 Renderer2D::s_CurrentBatchViewProjectionMatrix;
	BatchDrawType Renderer2D::s_CurrentBatchDrawType{ BatchDrawType::DEFAULT };

	bool Renderer2D::s_BatchActive{ false };

	uint32_t Renderer2D::s_DrawCallCount{ 0 };

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

		//AssetManager::PreloadShaders({
		//	Shaders::FlatColorQuad,
		//	Shaders::TexturedQuad
		//});

		CORI_CORE_INFO("Renderer2D: Initialization");

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
		
		//s_Shader_FlatColorQuad.reset(ShaderProgram::Create("assets/engine/shaders/flatColorQuadVert.glsl", "assets/engine/shaders/flatColorQuadFrag.glsl"));

		s_VertexArray_FlatColorQuad = VertexArray::Create();
		s_VertexBuffer_FlatColorQuad = VertexBuffer::Create();
		s_VertexBuffer_FlatColorQuad->SetLayout({
			{ ShaderDataType::Vec2, "a_Position" },
			{ ShaderDataType::Vec4, "a_Color" }
		});

		s_VertexBuffer_FlatColorQuad->Init(nullptr, s_MaxVertexCount * s_VertexBuffer_FlatColorQuad->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);

		s_VertexArray_FlatColorQuad->AddVertexBuffer(s_VertexBuffer_FlatColorQuad);


		s_IndexBuffer_FlatColorQuad = IndexBuffer::Create(indicesBatch, s_MaxIndexCount);
		s_VertexArray_FlatColorQuad->AddIndexBuffer(s_IndexBuffer_FlatColorQuad);

		s_VertexDataBuffer_FlatColorQuad = new QuadBatchVertexSetup[static_cast<size_t>(s_MaxIndexCount)];

		// textured quad

		//s_Shader_TexturedQuad.reset(ShaderProgram::Create("assets/engine/shaders/texturedQuadVert.glsl", "assets/engine/shaders/texturedQuadFrag.glsl"));

		s_VertexArray_TexturedQuad = VertexArray::Create();
		s_VertexBuffer_TexturedQuad = VertexBuffer::Create();
		s_VertexBuffer_TexturedQuad->SetLayout({
			{ ShaderDataType::Vec2, "a_Position" },
			{ ShaderDataType::Vec2, "a_TexturePosition" },
			{ ShaderDataType::Float, "a_Layer" }
		});

		s_VertexBuffer_TexturedQuad->Init(nullptr, s_MaxVertexCount * s_VertexBuffer_TexturedQuad->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
		s_VertexArray_TexturedQuad->AddVertexBuffer(s_VertexBuffer_TexturedQuad);

		s_IndexBuffer_TexturedQuad = IndexBuffer::Create(indicesBatch, s_MaxIndexCount);
		s_VertexArray_TexturedQuad->AddIndexBuffer(s_IndexBuffer_TexturedQuad);

		s_VertexDataBuffer_TexturedQuad = new TexturedQuadVertexSetup[static_cast<size_t>(s_MaxIndexCount)];

		CORI_CORE_INFO("Renderer2D: Initialization has been successful");

	}

	void Renderer2D::Shutdown() {
		delete[] s_VertexDataBuffer_FlatColorQuad;
	}

	// batching 

	void Renderer2D::BeginBatch(const glm::mat4& viewProjection, const glm::mat4& model) {
		CORI_PROFILE_FUNCTION();

		s_CurrentBatchModelMatrix = model;
		s_CurrentBatchViewProjectionMatrix = viewProjection;

		s_BatchActive = true;

		BeginBatch_FlatColorQuad();

		BeginBatch_TexturedQuad();
	}

	void Renderer2D::EndBatch() {
		CORI_PROFILE_FUNCTION();
		s_BatchActive = false;
		if (s_IndexCount_FlatColorQuad) {
			EndBatch_FlatColorQuad();
		}

		if (s_IndexCount_TexturedQuad) {
			EndBatch_TexturedQuad();
		}
	}

	void Renderer2D::ResetDebugStats() {
		s_DrawCallCount = 0;
	}

	uint32_t Renderer2D::GetDrawCallCount() {
		return s_DrawCallCount;
	}

	// VERY dangerous, dont use
	void Renderer2D::SetQuadsPerDraw(const uint32_t count) {
		s_MaxQuadCount = count;
		s_MaxVertexCount = s_MaxQuadCount * 4;
		s_MaxIndexCount = s_MaxQuadCount * 6;
	}

	void Renderer2D::NewBatch() {
		CORI_PROFILE_FUNCTION();
		EndBatch();
		s_BatchActive = true;
		BeginBatch_FlatColorQuad();
		BeginBatch_TexturedQuad();
	}

	// flat color quad

	void Renderer2D::BeginBatch_FlatColorQuad() {
		CORI_PROFILE_FUNCTION();
		s_VertexDataBufferPtr_FlatColorQuad = s_VertexDataBuffer_FlatColorQuad;
	}

	void Renderer2D::EndBatch_FlatColorQuad() {
		CORI_PROFILE_FUNCTION();
		auto size = reinterpret_cast<uint8_t*>(s_VertexDataBufferPtr_FlatColorQuad) - reinterpret_cast<uint8_t*>(s_VertexDataBuffer_FlatColorQuad);
		s_VertexBuffer_FlatColorQuad->Bind();
		s_VertexBuffer_FlatColorQuad->SetData(s_VertexDataBuffer_FlatColorQuad, size);

		// redundant shader binds, and subbufferdata calls 
		// overdraw is also a concern 

		std::shared_ptr<ShaderProgram> shader = AssetManager::GetShader(Shaders::FlatColorQuad);

		shader->Bind();
		shader->SetMat4("u_ViewProjection", s_CurrentBatchViewProjectionMatrix);
		shader->SetMat4("u_ModelMatrix", s_CurrentBatchModelMatrix);
		
		GraphicsCall::DrawElements(s_VertexArray_FlatColorQuad, s_IndexCount_FlatColorQuad);
		s_DrawCallCount++;

		s_IndexCount_FlatColorQuad = 0;
	}

	void Renderer2D::DrawQuad(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color) {
		if (CORI_CORE_ASSERT_ERROR(s_BatchActive, "You're trying to call DrawQuad, but you have not started a batch, it will not work.")) { return;  }
		if (s_IndexCount_FlatColorQuad >= s_MaxIndexCount) {
			NewBatch();
		}

		if (s_CurrentBatchDrawType != BatchDrawType::FLAT_COLOR_QUAD) {
			if (s_IndexCount_FlatColorQuad == 0) {
				s_CurrentBatchDrawType = BatchDrawType::FLAT_COLOR_QUAD;
			}
			else {
				NewBatch();
				s_CurrentBatchDrawType = BatchDrawType::FLAT_COLOR_QUAD;
			}
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
		CORI_PROFILE_FUNCTION();
		s_VertexDataBufferPtr_TexturedQuad = s_VertexDataBuffer_TexturedQuad;
	}

	void Renderer2D::EndBatch_TexturedQuad() {
		CORI_PROFILE_FUNCTION();
		auto size = reinterpret_cast<uint8_t*>(s_VertexDataBufferPtr_TexturedQuad) - reinterpret_cast<uint8_t*>(s_VertexDataBuffer_TexturedQuad);
		s_VertexBuffer_TexturedQuad->Bind();
		s_VertexBuffer_TexturedQuad->SetData(s_VertexDataBuffer_TexturedQuad, size);

		std::shared_ptr<ShaderProgram> shader = AssetManager::GetShader(Shaders::TexturedQuad);

		shader->Bind();
		shader->SetMat4("u_ViewProjection", s_CurrentBatchViewProjectionMatrix);
		shader->SetMat4("u_ModelMatrix", s_CurrentBatchModelMatrix);
		shader->SetInt("u_Texture", 0);

		s_CurrentTexture_TexturedQuad->Bind(0);
		GraphicsCall::DrawElements(s_VertexArray_TexturedQuad, s_IndexCount_TexturedQuad);
		s_DrawCallCount++;

		s_IndexCount_TexturedQuad = 0;
		//s_CurrentTexture_TexturedQuad.reset();
	}

	void Renderer2D::DrawQuad(const glm::vec2 position, const glm::vec2 size, const std::shared_ptr<Texture2D>& texture, const glm::vec2 UVmin, const glm::vec2 UVmax, const float layer) {

		if (CORI_CORE_ASSERT_ERROR(s_BatchActive, "You're trying to call DrawQuad, but you have not started a batch, it will not work.")) { return; }
		if (CORI_CORE_ASSERT_ERROR(texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
		if (s_IndexCount_TexturedQuad >= s_MaxIndexCount) {
			NewBatch();
		}

		if (s_CurrentBatchDrawType != BatchDrawType::TEXTURED_QUAD) {
			if (s_IndexCount_TexturedQuad == 0) {
				s_CurrentBatchDrawType = BatchDrawType::TEXTURED_QUAD;
			}
			else {
				NewBatch();
				s_CurrentBatchDrawType = BatchDrawType::TEXTURED_QUAD;
			}
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
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmin.x, UVmin.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x + size.x, position.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmax.x, UVmin.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x + size.x, position.y + size.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmax.x, UVmax.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x, position.y + size.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmin.x, UVmax.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_IndexCount_TexturedQuad += 6;

	}

	void Renderer2D::DrawQuadFlipped(const glm::vec2 position, const glm::vec2 size, const std::shared_ptr<Texture2D>& texture, const glm::vec2 UVmin, const glm::vec2 UVmax, const float layer) {

		if (CORI_CORE_ASSERT_ERROR(s_BatchActive, "You're trying to call DrawQuad, but you have not started a batch, it will not work.")) { return; }
		if (CORI_CORE_ASSERT_ERROR(texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
		if (s_IndexCount_TexturedQuad >= s_MaxIndexCount) {
			NewBatch();
		}

		if (s_CurrentBatchDrawType != BatchDrawType::TEXTURED_QUAD) {
			if (s_IndexCount_TexturedQuad == 0) {
				s_CurrentBatchDrawType = BatchDrawType::TEXTURED_QUAD;
			}
			else {
				NewBatch();
				s_CurrentBatchDrawType = BatchDrawType::TEXTURED_QUAD;
			}
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
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmax.x, UVmin.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x + size.x, position.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmin.x, UVmin.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x + size.x, position.y + size.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmin.x, UVmax.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_VertexDataBufferPtr_TexturedQuad->Position = { position.x, position.y + size.y };
		s_VertexDataBufferPtr_TexturedQuad->TexturePosition = { UVmax.x, UVmax.y };
		s_VertexDataBufferPtr_TexturedQuad->Layer = layer;
		s_VertexDataBufferPtr_TexturedQuad++;

		s_IndexCount_TexturedQuad += 6;

	}

	void Renderer2D::DrawQuad(const glm::vec2 position, const glm::vec2 size, const std::shared_ptr<Texture2D>& texture, const UVs& uvs, const float layer, bool flipped) {
		if (flipped) {
			DrawQuadFlipped(position, size, texture, uvs.UVmin, uvs.UVmax, layer);
		}
		else {
			DrawQuad(position, size, texture, uvs.UVmin, uvs.UVmax, layer);
		}

	}



	void Renderer2D::DrawQuad(const glm::vec2 position, const glm::vec2 size, const std::shared_ptr<SpriteAtlas>& atlas, uint32_t index) {
		DrawQuad(position, size, atlas->GetTexture(), atlas->GetSpriteUVsAtIndex(index), 5.0f);
	}

	void Renderer2D::DrawQuad(const glm::vec2 position, const glm::vec2 size, const std::shared_ptr<Sprite>& sprite) {
		DrawQuad(position, size, sprite->GetTexture(), sprite->GetUVs(), 5.0f);
	}

}