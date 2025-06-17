#define CORI_PIXELS_PER_METER 16
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
		static bool b2draw = false;

		if (b2draw) {
			Cori::ImGuiPresets::Box2dDebugDraw({ 640, 360 }, CORI_PIXELS_PER_METER, this, true, 2000.0f);
		}

		ImGui::Begin("Test");

		static int a = 0;

		if (ImGui::Button("Add box")) {
			a++;
			auto ent = ActiveScene->CreateEntity("Test Entity " + std::to_string(a));
			ent.AddComponent<Cori::RenderingComponent>(glm::vec2{ 16.0f, 16.0f });
			ent.AddComponent<Cori::PositionComponent>(glm::vec2{ 50.0f * a, 200.0f });
			auto atlas = Cori::AssetManager::GetSpriteAtlas(Cori::SpriteAtlases::Atlas);
			ent.AddComponent<Cori::SpriteComponent>(atlas->GetTexture(), atlas->GetSpriteUVsAtIndex(a));
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

		if (ImGui::Button("Create 'Test2' Scene")) {
			Cori::SceneManager::CreateScene("Test2 Scene");
			BindScene("Test2 Scene");
			ActiveScene->ActiveCamera.CreateOrthoCamera(0, 640, 0, 360);
		}

		if (ImGui::Button("Create 'Test' Scene")) {
			Cori::SceneManager::CreateScene("Test Scene");
			BindScene("Test Scene");
			ActiveScene->ActiveCamera.CreateOrthoCamera(0, 640, 0, 360);
		}

		if (ImGui::Button("box2d draw")) {
			b2draw = !b2draw;
		}

		if (ImGui::Button("Add b2Box")) {
			auto ent = ActiveScene->CreateEntity();

			Cori::Physics::Body::Params bp;
			bp.type = b2_dynamicBody;
			bp.position = { 4.0f, 4.0f };

			auto& rb = ent.AddComponent<Cori::Physics::Rigidbody_EntityComponent>(ActiveScene->PhysicsWorld, bp);
			
			Cori::Physics::Shape::Params sp;

			rb.CreateShape(Cori::Physics::DestroyWithParent, sp, Cori::Physics::Polygon::CreateBox({ 1.0f, 1.0f }));
		}

		if (ImGui::Button("Add Floor")) {
			auto ent = ActiveScene->CreateEntity("Floor");

			Cori::Physics::Body::Params bp;
			bp.type = b2_staticBody;
			bp.position = { 0.0f, 0.0f };

			auto& rb = ent.AddComponent<Cori::Physics::Rigidbody_EntityComponent>(ActiveScene->PhysicsWorld, bp);

			Cori::Physics::Shape::Params sp;

			rb.CreateShape(Cori::Physics::DestroyWithParent, sp, Cori::Physics::Polygon::CreateBox({ 40.0f, 1.0f }));
		}

		ImGui::End();
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