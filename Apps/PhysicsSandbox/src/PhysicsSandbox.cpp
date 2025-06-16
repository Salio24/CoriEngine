#include <Cori.hpp>
#include <CoriEntry.hpp>

namespace Cori {
	namespace Texture2Ds {
		inline const Texture2DDescriptor AtlasTexture{
			"Test AtlasTexture",
			"assets/engine/textures/testTileset32.png"
		};
	}

	namespace SpriteAtlases {
		inline const SpriteAtlasDescriptor Atlas{
			"Test Atlas",
			Texture2Ds::AtlasTexture,
			{32, 32}
		};

	}
}

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

		static bool b2draw = false;

		if (ImGui::Button("Add box")) {
			a++;
			auto ent = ActiveScene->CreateEntity("Test Entity " + std::to_string(a));
			ent.AddComponent<Cori::RenderingComponent>(glm::vec2{ 50.0f, 50.0f });
			ent.AddComponent<Cori::PositionComponent>(glm::vec2{ 50.0f * a, 500.0f });
			auto atlas = Cori::AssetManager::GetSpriteAtlas(Cori::SpriteAtlases::Atlas);
			ent.AddComponent<Cori::SpriteComponent>(atlas->GetTexture(), atlas->GetSpriteUVsAtIndex(a));
		}

		if (ImGui::Button("Create 'Test2' Scene")) {
			Cori::SceneManager::CreateScene("Test2 Scene");
		}

		if (ImGui::Button("Create 'Test' Scene")) {
			Cori::SceneManager::CreateScene("Test Scene");
		}

		if (ImGui::Button("Set Camera")) {
			ActiveScene->ActiveCamera.CreateOrthoCamera(0, 7680, 0, 4320);
		}

		if (ImGui::Button("Destroy Test2")) {
			Cori::SceneManager::DestroyScene("Test2 Scene");
		}

		if (ImGui::Button("Destroy Test")) {
			Cori::SceneManager::DestroyScene("Test Scene");
		}

		if (ImGui::Button("Unbind Scene")) {
			UnbindScene();
		}

		if (ImGui::Button("Test2 bind")) {
			BindScene("Test2 Scene");
		}

		if (ImGui::Button("Test bind")) {
			BindScene("Test Scene");
		}

		if (ImGui::Button("box2d draw")) {
			b2draw = !b2draw;
		}

		ImGui::End();

		if (b2draw) {
			//debug_renderer.DrawShapes(world); // Draw Box2D shapes.
			//debug_renderer.DrawModeToggles(); // Checkboxes for adjusting visualization.
		}
	}

	void OnUpdate(const double deltaTime) override {


	}

	virtual void OnTickUpdate(const float timeStep) override {

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