#include <iostream>
#include <Cori.hpp>
#include <imgui.h>
#include <memory>


class ExampleLayer : public Cori::Layer {
public:
	ExampleLayer() : Layer("Example") { 

	}
	
	void OnEvent(Cori::Event& event) override {
		if (!event.IsOfType(Cori::EventType::MouseMoved)) {
			CORI_TRACE("| Layer: {0} | Event: {1}", this->GetName(), event);
		}
	}

	virtual void OnImGuiRender() override {

	}

	void OnUpdate() override {
		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_H)) {
			CORI_WARN("test");
		}
		
	}
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