//#define CORI_ASYNC_LOGGING
//#define CORI_NO_FILE_LOGGING
#include <Cori.hpp>
#include <CoriEntry.hpp>

class ExampleLayer : public Cori::Core::Layer {
public:
	ExampleLayer() : Layer("Example") {
		Cori::World::SceneManager::CreateScene("Test Scene");
		BindScene("Test Scene");
		ActiveScene.GetActiveCamera().CreateOrthoCamera(0, 2560, 0, 1080, 50);

	}

	~ExampleLayer() {

	}

	void OnEvent(Cori::Core::Event& event) override {

		Cori::Core::EventDispatcher dispatcher(event);

	}

	void OnImGuiRender(Cori::Core::GameTimer& gameTimer) override {
		ImGui::Begin("Test");

		if (ImGui::Button("Test Button")) {
			CORI_DEBUG("Test Button");
		}

		ImGui::End();

		Cori::ImGuiPresets::ScreenModeAndResolutionDropdowns();


		//Cori::ImGuiPresets::ScreenModeAndResolutionDropdowns();
	}

	void OnUpdate(Cori::Core::GameTimer& gameTimer) override {

	}

	void OnTickUpdate(Cori::Core::GameTimer& gameTimer) override {


	}
};

class Sandbox : public Cori::Core::Application {
public:
	Sandbox(): Application("renderer sandbox") {
		PushLayer(new ExampleLayer());

		CORI_INFO("Sandbox application created");
	}

	~Sandbox() {
		CORI_INFO("Sandbox application destroyed");
	}
};

Cori::Core::Application* Cori::Core::CreateApplication() {
	return new Sandbox();
}