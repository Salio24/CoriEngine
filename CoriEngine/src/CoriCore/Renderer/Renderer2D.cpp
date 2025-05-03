#include "Renderer2D.hpp"

namespace Cori {
	
	static OrtoCamera s_Camera;

	static std::shared_ptr<ShaderProgram> s_Shader;
	static std::shared_ptr<VertexArray> s_VertexArray;
	static std::shared_ptr<VertexBuffer> s_VertexBuffer;
	static std::shared_ptr<IndexBuffer> s_IndexBuffer;
	static std::shared_ptr<PipelineProgram> s_Pipeline;

	void Renderer2D::Init() {
		s_VertexArray.reset(VertexArray::Create());
		s_VertexBuffer.reset(VertexBuffer::Create(nullptr, 4 * (sizeof(glm::vec4) + sizeof(glm::vec2)), DRAW_TYPE::DYNAMIC));

		s_VertexBuffer->SetLayout({
			{ ShaderDataType::Vec2, "a_Position" },
			{ ShaderDataType::Vec4, "a_Color" }
			});

		s_VertexArray->AddVertexBuffer(s_VertexBuffer);

		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
		s_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		s_VertexArray->AddIndexBuffer(s_IndexBuffer);

		s_Shader.reset(ShaderProgram::Create("C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/CoriEngine/src/CoriCore/Renderer/ShaderTemp/defaultVert.glsl", "C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/CoriEngine/src/CoriCore/Renderer/ShaderTemp/defaultFrag.glsl"));
		
		s_Pipeline.reset(PipelineProgram::Create());

		s_Camera = OrtoCamera(glm::vec4(0.0f, 1920.0f, 0.0f, 1080.0f));
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
		float vertices[6 * 4] = {
			position.x,          position.y,          color.r, color.g, color.b, color.a,
			position.x + size.x, position.y,          color.r, color.g, color.b, color.a,
			position.x + size.x, position.y + size.y, color.r, color.g, color.b, color.a,
			position.x,          position.y + size.y, color.r, color.g, color.b, color.a
		};

		s_VertexBuffer->Bind();
		s_VertexBuffer->SetData(vertices, sizeof(vertices));

		s_Pipeline->Bind();
		s_Pipeline->BindShaderProgram(s_Shader);
		s_Shader->SetMat4("u_ViewProjection", s_Camera.GetProjectionMatrix());

		s_VertexArray->Bind();
		Renderer::GetCoriGraphicsAPI()->DrawElements(s_VertexArray);
		s_VertexArray->Unbind();

	}

}