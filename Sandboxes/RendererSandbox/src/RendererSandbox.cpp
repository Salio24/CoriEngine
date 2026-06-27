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
		Cori::Graphics::SceneRenderer::CreateInfo info{
			.initialPRTExtent = { 1920, 1080 },
			.PRTFormat = vk::Format::eR8G8B8A8Srgb,
			.name = "Test renderer",
			.registerPRTWithImGui = false
		};

		ActiveScene.RegisterSystem<Cori::World::Systems::RenderSync>(std::move(info));
		auto swordMaterial = Cori::Core::AssetManager2::Load<Cori::Graphics::Material>("assets/Sword_Material.json");
		auto swordMesh = Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>("assets/Sword_M.json");
		auto entity = ActiveScene.CreateEntity("Test ent");
		entity.AddComponent<Cori::World::Components::Entity::Rendering>(std::move(swordMesh), std::move(swordMaterial), glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });
		auto& tc = entity.GetComponents<Cori::World::Components::Entity::Transform>();
		tc.SetLocalScale({ 0.5f, 0.5f, 0.5f });

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