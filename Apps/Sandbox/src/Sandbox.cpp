#include <iostream>
#include <Cori.hpp>
#include <memory>


class ExampleLayer : public Cori::Layer {
public:
	ExampleLayer() : Layer("Example") { 

	}

	void OnEvent(Cori::Event& event) override {
		//CORI_TRACE("{0}", event);
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