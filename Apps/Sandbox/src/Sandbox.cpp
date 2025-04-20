#include <iostream>
#include <Cori.hpp>
#include <imgui.h>
#include "../../src/CoriCore/ImGui/IGWrapper.hpp"

class ExampleLayer : public Cori::Layer {
public:
	ExampleLayer() : Layer("Example") { 

	}

	void OnEvent(Cori::Event& event) override {
		CORI_TRACE("{0}", event);
	}

	virtual void OnImGuiRender() override {
		//IG::test();
		IG::Begin("Example Layer");
		IG::Text("Hello from ExampleLayer");
		IG::End();

		//Cori::Application::test();
		// create a wrapper for imgui to call all of its funcs inside dll
		// vvv THIS WILL NOT WORK
		//ImGui::Begin("Example Layer");
		//ImGui::Text("Hello from ExampleLayer");
		//ImGui::End();

		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_H)) {
			CORI_DEBUG("TEST");
		}

	}

	void OnUpdate() override {
		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_H)) {
			CORI_WARN("TEST");
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