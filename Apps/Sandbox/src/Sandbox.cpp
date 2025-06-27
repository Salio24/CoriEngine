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

class CustomEvent : public Cori::Event {
public:
	CustomEvent(const std::string& somedata) : m_Data(somedata) {}

	std::string ToString() const override {
		return "UDE";
	}

	inline std::string& GetData() {
		return m_Data;
	}

	EVENT_CLASS_TYPE(GameUserDefinedEvent)
	EVENT_CLASS_CATEGORY(Cori::EventCategoryGameplay)
private:
	std::string m_Data;
};


class ExampleLayer : public Cori::Layer {
public:
	ExampleLayer() : Layer("Example") { 
		Cori::GraphicsCall::SetViewport(0, 0, Cori::Application::GetWindow().GetWidth(), Cori::Application::GetWindow().GetHeight());
	}

	~ExampleLayer() {

	}
	
	virtual void OnEvent(Cori::Event& event) override {

		Cori::EventDispatcher dispatcher(event);
		dispatcher.Dispatch<CustomEvent>([](CustomEvent& e) -> bool {
			CORI_DEBUG("Event data {}", e.GetData());
			return true;
			
		});


		if (!event.IsOfType(Cori::EventType::MouseMoved)) {
			CORI_TRACE("| Layer: {0} | Event: {1}", this->GetName(), event);
		}
	}

	virtual void OnImGuiRender(const double deltaTime) override {
		ImGui::Begin("Test");

		static int a = 0;

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

		if (ImGui::Button("Crash")) {
			ActiveScene->GetNamedEntity("Crash Entity");
		}

		if (ImGui::Button("Assert")) {
			CORI_CORE_ASSERT_DEBUG(false, "Message: {0}", std::string("test"));
		}

		ImGui::End();
	}

	void OnUpdate(const double deltaTime, const double tickAlpha) override {


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