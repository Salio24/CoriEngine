#include <iostream>
#include <Cori.hpp>
#include <imgui.h>
#include <memory>
#include <glm/glm.hpp>

class ExampleLayer : public Cori::Layer {
public:
	ExampleLayer() : Layer("Example") { 

		//m_VertexArray.reset(Cori::VertexArray::Create());
		//
		//float vertices[3 * 7] = {
		//	1.0f, 1.0f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
		//	2.0f, 3.0f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
		//	3.0f, 1.0f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f
		//};
		//
		//std::shared_ptr<Cori::VertexBuffer> vertexBuffer;
		//vertexBuffer.reset(Cori::VertexBuffer::Create(vertices, sizeof(vertices), Cori::DRAW_TYPE::STATIC));
		//
		//vertexBuffer->SetLayout({
		//	{ Cori::ShaderDataType::Vec3, "a_Position" },
		//	{ Cori::ShaderDataType::Vec4, "a_Color" }
		//	});
		//
		//m_VertexArray->AddVertexBuffer(vertexBuffer);
		//
		//uint32_t indices[3] = { 0, 1, 2 };
		//std::shared_ptr<Cori::IndexBuffer> indexBuffer;
		//indexBuffer.reset(Cori::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		//m_VertexArray->AddIndexBuffer(indexBuffer);
		//
		//m_Shader.reset(Cori::ShaderProgram::Create("C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/CoriEngine/src/CoriCore/Renderer/ShaderTemp/defaultVert.glsl", "C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/CoriEngine/src/CoriCore/Renderer/ShaderTemp/defaultFrag.glsl"));
		//
		//m_Pipeline.reset(Cori::PipelineProgram::Create());

		Cori::Renderer2D::Init();
	}
	
	void OnEvent(Cori::Event& event) override {
		if (!event.IsOfType(Cori::EventType::MouseMoved)) {
			CORI_TRACE("| Layer: {0} | Event: {1}", this->GetName(), event);
		}
	}

	virtual void OnImGuiRender() override {

	}

	void OnUpdate() override {

		Cori::Renderer::GetCoriGraphicsAPI()->SetViewport(0, 0, Cori::Application::Get().GetWindow().GetWidth(), Cori::Application::Get().GetWindow().GetHeight());
		Cori::Renderer::GetCoriGraphicsAPI()->SetClearColor({ 1.0f, 1.0f, 0.0f, 1.0f });
		Cori::Renderer::GetCoriGraphicsAPI()->ClearFramebuffer();

		Cori::Renderer2D::DrawQuad(glm::vec2(50.0f, 50.0f), glm::vec2(50.0f, 50.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));

		//m_Pipeline->Bind();
		//m_Pipeline->BindShaderProgram(m_Shader);
		//m_Shader->SetMat4("u_ViewProjection", m_Camera.GetProjectionMatrix());
		//Cori::Renderer::Render(m_VertexArray);

		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_H)) {
			CORI_WARN("test");
		}
	}

	//Cori::OrtoCamera m_Camera = Cori::OrtoCamera(glm::vec4(0.0f, 4.0f, 0.0f, 4.0f));



	//std::shared_ptr<Cori::ShaderProgram> m_Shader;
	//std::shared_ptr<Cori::VertexArray> m_VertexArray;
	//std::shared_ptr<Cori::PipelineProgram> m_Pipeline;
};

class Sandbox : public Cori::Application {
public:
	Sandbox() {

		PushLayer(new ExampleLayer());

		CORI_INFO("Sandbox application created");
	}
	~Sandbox() {
		CORI_INFO("Sandbox application destroyed");
	}
};

Cori::Application* Cori::CreateApplication() {
	return new Sandbox();
}