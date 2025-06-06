#include <iostream>
#include <Cori.hpp>
#include <imgui.h>
#include <memory>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

class ExampleLayer : public Cori::Layer {
public:
	ExampleLayer() : Layer("Example") { 
		Cori::GraphicsCall::SetViewport(0, 0, Cori::Application::GetWindow().GetWidth(), Cori::Application::GetWindow().GetHeight());
	}

	~ExampleLayer() {

	}
	
	virtual void OnEvent(Cori::Event& event) override {

		Cori::EventDispatcher dispatcher(event);

		if (!event.IsOfType(Cori::EventType::MouseMoved)) {
			CORI_TRACE("| Layer: {0} | Event: {1}", this->GetName(), event);
		}
	}

	virtual void OnImGuiRender(const double deltaTime) override {
		ImGui::Begin("Test");

		static int a = 0;

		if (ImGui::Button("Add box")) {
			a++;
			auto ent = Cori::SceneManager::ActiveScene::CreateEntity("Test Entity " + std::to_string(a));
			ent.AddComponent<Cori::RenderingComponent>(glm::vec2{50.0f, 50.0f});
			ent.AddComponent<Cori::PositionComponent>(glm::vec2{ 50.0f * a, 500.0f });
		}

		if (ImGui::Button("Create Trigger")) {
			auto other = Cori::SceneManager::ActiveScene::CreateEntity("Other Entity");
			other.AddComponent<Cori::RenderingComponent>(glm::vec2{ 50.0f, 50.0f });
			other.AddComponent<Cori::ColliderComponent>();
			other.AddComponent<Cori::PositionComponent>(glm::vec2{ 50.0f, 50.0f });
			other.AddComponent<Cori::PlayerTagComponent>();

			auto trigger = Cori::SceneManager::ActiveScene::CreateEntity("Trigger Entity");
			trigger.AddComponent<Cori::ColliderComponent>();
			trigger.AddComponent<Cori::PositionComponent>(glm::vec2{ 50.0f, 50.0f });
			trigger.AddComponent<Cori::TriggerComponent>([](Cori::Entity& trigger, Cori::Entity& entity) -> bool {
				// trigger logic/script
				CORI_WARN("trigger");
				return true;
			});
		}

		if (ImGui::Button("Activate Trigger")) {
			Cori::TriggerManager::Get().Test();
		}

		if (ImGui::Button("Create 'Test2' Scene")) {
			Cori::SceneManager::CreateScene("Test2 Scene");
		}

		if (ImGui::Button("Create 'Test' Scene")) {
			Cori::SceneManager::CreateScene("Test Scene");
		}

		if (ImGui::Button("Set Camera")) {
			Cori::CameraController::CreateOrthoCamera(0, 7680, 0, 4320);
		}

		if (ImGui::Button("Destroy Test2")) {
			Cori::SceneManager::DestroyScene("Test2 Scene");
		}

		if (ImGui::Button("Destroy Test")) {
			Cori::SceneManager::DestroyScene("Test Scene");
		}

		if (ImGui::Button("Unbind Scene")) {
			Cori::SceneManager::UnbindScene();
		}

		if (ImGui::Button("Test2 bind")) {
			Cori::SceneManager::BindScene("Test2 Scene");
		}

		if (ImGui::Button("Test bind")) {
			Cori::SceneManager::BindScene("Test Scene");
		}

		if (ImGui::Button("Crash")) {
			Cori::SceneManager::ActiveScene::GetNamedEntity("Crash Entity");
		}

		ImGui::End();
	}

	void OnUpdate(const double deltaTime) override {

		Cori::GraphicsCall::SetClearColor({ 1.0f, 1.0f, 0.0f, 1.0f });
		Cori::GraphicsCall::ClearFramebuffer();

	}

	virtual void OnTickUpdate() override {
		
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