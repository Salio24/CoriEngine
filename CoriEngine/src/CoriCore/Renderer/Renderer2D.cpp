#include "Renderer2D.hpp"
#include "GraphicsCall.hpp"

namespace Cori {
	
	OrtoCamera Renderer2D::s_Camera;

	std::shared_ptr<ShaderProgram> Renderer2D::s_Shader;
	std::shared_ptr<PipelineProgram> Renderer2D::s_Pipeline;

	std::shared_ptr<VertexArray> Renderer2D::s_VertexArray_separate;
	std::shared_ptr<VertexBuffer> Renderer2D::s_VertexBuffer_separate;
	std::shared_ptr<IndexBuffer> Renderer2D::s_IndexBuffer_separate;

	std::shared_ptr<VertexArray> Renderer2D::s_VertexArray_batch;
	std::shared_ptr<VertexBuffer> Renderer2D::s_VertexBuffer_batch;
	std::shared_ptr<IndexBuffer> Renderer2D::s_IndexBuffer_batch;

	glm::mat4 Renderer2D::s_CurrentBatchModelMatrix;
	uint32_t Renderer2D::s_QuadIndexCount{ 0 };

	QuadBatchVertexSetup* Renderer2D::s_QuadBuffer{ nullptr };
	QuadBatchVertexSetup* Renderer2D::s_QuadBufferPtr{ nullptr };


	void Renderer2D::Init() {
		// separate camera/shader for each renderer type
		s_Shader.reset(ShaderProgram::Create("C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/CoriEngine/src/CoriCore/Renderer/ShaderTemp/defaultVert.glsl", "C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/CoriEngine/src/CoriCore/Renderer/ShaderTemp/defaultFrag.glsl"));

		s_Pipeline.reset(PipelineProgram::Create());

		s_Camera.SetCameraSize(0, 1920, 0, 1080);

		// separate renderer (1 draw call per object)
		s_VertexArray_separate.reset(VertexArray::Create());
		s_VertexBuffer_separate.reset(VertexBuffer::Create());
		s_VertexBuffer_separate->SetLayout({
			{ ShaderDataType::Vec2, "a_Position" },
			{ ShaderDataType::Vec4, "a_Color" }
		});

		s_VertexBuffer_separate->Init(nullptr, 4 * s_VertexBuffer_separate->GetLayout().GetStrinde(), DRAW_TYPE::DYNAMIC);
		s_VertexArray_separate->AddVertexBuffer(s_VertexBuffer_separate);

		uint32_t indicesSeparate[6] = { 0, 1, 2, 2, 3, 0 };
		s_IndexBuffer_separate.reset(IndexBuffer::Create(indicesSeparate, sizeof(indicesSeparate)));
		s_VertexArray_separate->AddIndexBuffer(s_IndexBuffer_separate);

		// batch renderer
		s_VertexArray_batch.reset(VertexArray::Create());
		s_VertexBuffer_batch.reset(VertexBuffer::Create());
		s_VertexBuffer_batch->SetLayout({
			{ ShaderDataType::Vec2, "a_Position" },
			{ ShaderDataType::Vec4, "a_Color" }
		});

		s_VertexBuffer_batch->Init(nullptr, s_MaxQuadCount * s_VertexBuffer_batch->GetLayout().GetStrinde(), DRAW_TYPE::DYNAMIC);

		s_VertexArray_batch->AddVertexBuffer(s_VertexBuffer_batch);

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

		s_IndexBuffer_batch.reset(IndexBuffer::Create(indicesBatch, sizeof(indicesBatch)));
		s_VertexArray_batch->AddIndexBuffer(s_IndexBuffer_batch);

		s_QuadBuffer = new QuadBatchVertexSetup[s_MaxIndexCount];
	}

	void Renderer2D::Shutdown() {
		delete[] s_QuadBuffer;
	}

	void Renderer2D::ResizeCamera(int left, int right, int bottom, int top) {
		s_Camera.SetCameraSize(left, right, bottom, top);
	}

	void Renderer2D::DrawQuadSeparate(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color, const glm::mat4& modelMatrix) {
		float vertices[6 * 4] = {
			position.x,          position.y,          color.r, color.g, color.b, color.a,
			position.x + size.x, position.y,          color.r, color.g, color.b, color.a,
			position.x + size.x, position.y + size.y, color.r, color.g, color.b, color.a,
			position.x,          position.y + size.y, color.r, color.g, color.b, color.a
		};

		s_VertexBuffer_separate->Bind();
		s_VertexBuffer_separate->SetData(vertices, sizeof(vertices));

		s_Pipeline->Bind();
		s_Pipeline->BindShaderProgram(s_Shader);
		s_Shader->SetMat4("u_ViewProjection", s_Camera.GetProjectionMatrix());
		s_Shader->SetMat4("u_ModelMatrix", modelMatrix);

		s_VertexArray_separate->Bind();
		GraphicsCall::DrawElements(s_VertexArray_separate);
		s_VertexArray_separate->Unbind();
	}

	void Renderer2D::BeginBatch(const glm::mat4& model) {
		s_QuadBufferPtr = s_QuadBuffer;
		s_CurrentBatchModelMatrix = model;
	}

	void Renderer2D::EndBatch() {
		auto size = reinterpret_cast<uint8_t*>(s_QuadBufferPtr) - reinterpret_cast<uint8_t*>(s_QuadBuffer);
		s_VertexBuffer_batch->Bind();
		s_VertexBuffer_batch->SetData(s_QuadBuffer, size);
		Flush();
	}

	void Renderer2D::Flush() {
		s_Pipeline->Bind();
		s_Pipeline->BindShaderProgram(s_Shader);
		s_Shader->SetMat4("u_ViewProjection", s_Camera.GetProjectionMatrix());
		s_Shader->SetMat4("u_ModelMatrix", s_CurrentBatchModelMatrix);

		s_VertexArray_batch->Bind();
		GraphicsCall::DrawElements(s_VertexArray_batch);
		s_VertexArray_batch->Unbind();
		s_QuadIndexCount = 0;
	}

	void Renderer2D::DrawQuadBatch(const glm::vec2 position, const glm::vec2 size, const glm::vec4& color) {
		if (s_QuadIndexCount >= s_MaxIndexCount) {
			EndBatch();
			BeginBatch(s_CurrentBatchModelMatrix);
		}

		s_QuadBufferPtr->Position = { position.x, position.y };
		s_QuadBufferPtr->Color = color;
		s_QuadBufferPtr++;

		s_QuadBufferPtr->Position = { position.x + size.x, position.y };
		s_QuadBufferPtr->Color = color;
		s_QuadBufferPtr++;

		s_QuadBufferPtr->Position = { position.x + size.x, position.y + size.y };
		s_QuadBufferPtr->Color = color;
		s_QuadBufferPtr++;

		s_QuadBufferPtr->Position = { position.x, position.y + size.y };
		s_QuadBufferPtr->Color = color;
		s_QuadBufferPtr++;

		s_QuadIndexCount += 6;
	}
}