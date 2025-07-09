#define CORI_ASYNC_LOGGING
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

		static Cori::Entity ent;

		if (ImGui::Button("Setup Scene")) {
			Cori::SceneManager::CreateScene("Test Scene");
			BindScene("Test Scene");
			ActiveScene->ActiveCamera.CreateOrthoCamera(0, 7680, 0, 4320);
		}


		if (ImGui::Button("Create Ent")) {
			ent = ActiveScene->CreateEntity("Test");
			auto& rend = ent.AddComponent<Cori::Components::Entity::RenderGroup>(ActiveScene.get(), ent);
			rend.SetWorldPosition({50.0f, 50.0f});
		}


		if (ImGui::Button("Add Primitive")) {
			auto& rend = ent.GetComponents<Cori::Components::Entity::RenderGroup>();

			a++;

			auto atlas = Cori::AssetManager::GetSpriteAtlas(Cori::SpriteAtlases::Atlas);

			auto uvs = atlas->GetSpriteUVsAtIndex(23 + a);

			Cori::Graphics::QuadPrimitive::Descriptor desc;
			desc.localPosition = {50.0f * a, 50.0f * a};
			desc.size = {100, 100};
			desc.layer = 3;
			desc.texture = atlas->GetTexture();
			desc.uvs = uvs;

			rend.AddPrimitive<Cori::Graphics::QuadPrimitive>(desc);
		}

		if (ImGui::Button("Invalidate")) {
			auto& pool = ActiveScene->GetPoolForType<Cori::Graphics::QuadPrimitive>();
			pool.InvalidatePrimitive(2);
			pool.InvalidatePrimitive(5);
			pool.InvalidatePrimitive(11);
			pool.InvalidatePrimitive(9);
			pool.InvalidatePrimitive(8);
			pool.InvalidatePrimitive(3);
			pool.InvalidatePrimitive(6);
		}

		if (ImGui::Button("Defrag")) {
			auto& pool = ActiveScene->GetPoolForType<Cori::Graphics::QuadPrimitive>();
			pool.Defragment();
		}

		ImGui::Text("FPS: %.2f", fps);
		ImGui::Text("FPS 10s avg: %.2f", fps10);


		ImGui::End();
	}

	void OnUpdate(const double deltaTime, const double tickAlpha) override {
		accum++;
	}

	virtual void OnTickUpdate(const float timeStep) override {
		static uint8_t tic = 0;
		tic++;
		static uint8_t tic10 = 0;
		if (tic == 60) {
			fps = (float)accum;
			accum10+= accum;
			accum = 0.0f;
			tic = 0;
			tic10++;
		}
		if (tic10 == 10) {
			fps10 = (float)accum10 / 10.0f;
			accum10 = 0.0f;
			tic10 = 0;
		}

	}

	float fps;
	float fps10;
	uint32_t accum{ 0 };
	uint32_t accum10{ 0 };

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