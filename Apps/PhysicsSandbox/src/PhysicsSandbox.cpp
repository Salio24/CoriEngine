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
			debug_renderer.ViewportCalc({ 640, 360 });
			debug_renderer.DrawShapes(ActiveScene->PhysicsWorld);
			if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive())
			{
				debug_renderer.HandleMouseDrag(ActiveScene->PhysicsWorld);
			}
			debug_renderer.DrawModeToggles();
		}

		ImGui::Begin("Test");

		static int a = 0;

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
			ActiveScene->ActiveCamera.CreateOrthoCamera(0, 640, 0, 360);
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

		if (ImGui::Button("test")) {
			auto player = ActiveScene->CreateEntity("Player");

			Cori::Physics::Body::Params bp;
			bp.type = b2_dynamicBody;
			bp.position = { 20.0f, 20.0f };

			auto& rb = player.AddComponent<Cori::Physics::Rigidbody_EntityComponent>(ActiveScene->PhysicsWorld, bp);

			CORI_INFO("Cent: {}, {}", rb.GetMassData().center.x, rb.GetMassData().center.y);

			CORI_INFO("Pos: {}, {}", rb.GetPosition().x, rb.GetPosition().y);
			
			Cori::Physics::Shape::Params sp;
			sp.density = 5.0f;

			rb.CreateShape(Cori::Physics::DestroyWithParent, sp, Cori::Physics::Polygon::CreateBox({ 4.0f, 4.0f }));

			CORI_INFO("Cent: {}, {}", rb.GetWorldCenterOfMass().x, rb.GetWorldCenterOfMass().y);

			CORI_INFO("Pos: {}, {}", rb.GetPosition().x, rb.GetPosition().y);
		}


		if (ImGui::Button("Tst2")) {
			auto p = ActiveScene->GetNamedEntity("Player");
			auto& rb = p.GetComponents<Cori::Physics::Rigidbody_EntityComponent>();
			

			CORI_INFO("Cent: {}, {}", rb.GetWorldCenterOfMass().x, rb.GetWorldCenterOfMass().y);


			CORI_INFO("Pos: {}, {}", rb.GetPosition().x, rb.GetPosition().y);
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