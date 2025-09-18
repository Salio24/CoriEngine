#define CORI_ASYNC_LOGGING
#define CORI_NO_FILE_LOGGING
#include <Cori.hpp>
#include <CoriEntry.hpp>

CORI_DECLARE_TAG(Test);

inline const Cori::Audio::Sound::Descriptor Sound1{
	"test1",
	"Troubadeck 01 A Simple Snail.ogg"
};

inline const Cori::Audio::Sound::Descriptor Sound2{
	"test2",
	"Troubadeck 02 Leapfrogs.ogg"
};


class ExampleLayer : public Cori::Core::Layer {
public:
	ExampleLayer() : Layer("Example") { 


		Cori::World::SceneManager::CreateScene("Test Scene");
		BindScene("Test Scene");
		ActiveScene.GetActiveCamera().CreateOrthoCamera(0, 2560, 0, 1080, -50, 0);
		track = Cori::Audio::Track::Create("Test");
		sound = Cori::AssetManager::Get(Sound1);
	}

	~ExampleLayer() {

	}
	
	virtual void OnEvent(Cori::Core::Event& event) override {

		Cori::Core::EventDispatcher dispatcher(event);


		if (!event.IsOfType(Cori::Core::EventType::MouseMoved)) {
			CORI_TRACE("| Layer: {0} | Event: {1}", this->GetName(), event);
		}
	}

	template<typename T>
	void TestAsser() {
		T def{};
		CORI_CORE_ASSERT(false, "test", def);
		//CORI_CORE_ASSERT_DEBUG(false, "lol {}", def);
	}

	virtual void OnImGuiRender(const double deltaTime) override {
		ImGui::Begin("Test");

		static int a = 0;


		static bool ale = false;

		if (ale) {

			ale = false;
		}

		if (ImGui::Button("Trace")) {
			CORI_PROFILE_REQUEST_NEXT_FRAME();
			ale = true;
		}



		if (ImGui::Button("Draw hier")) {
			auto player = ActiveScene.GetEntityFromCache("player"_hs32);
			if (player) {
				player.value().PrintHierarchy();
			} else {
				player.error().ignore();
			}
		}



		if (ImGui::Button("Play")) {
			track->Start();
		}
		if (ImGui::Button("Test1")) {
			track->SetSound(sound);
		}

		if (ImGui::Button("Test2")) {
			track->SetSound(Cori::AssetManager::Get(Sound2));
		}

		if (ImGui::Button("Stop")) {
			track->Stop(300);
		}

		if (ImGui::Button("callback")) {


		}






		ImGui::Text("FPS: %.2f", fps);
		ImGui::Text("FPS 10s avg: %.2f", fps10);


		ImGui::End();


		Cori::ImGuiPresets::ScreenModeAndResolutionDropdowns();
	}

	void OnUpdate(const Cori::Core::GameTimer& gameTimer) override {
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

	std::shared_ptr<Cori::Audio::Track> track;
	std::shared_ptr<Cori::Audio::Sound> sound;

	float fps;
	float fps10;
	uint32_t accum{ 0 };
	uint32_t accum10{ 0 };

};

class Sandbox : public Cori::Core::Application {
public:
	Sandbox(): Application("sandbox") {
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