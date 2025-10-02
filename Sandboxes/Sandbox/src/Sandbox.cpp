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

struct Testttt {
	uint32_t id = 123;
	float fps = 5;
	uint32_t accum = 1;
	float fps10 = 856;
	uint32_t accum10 = 123;
	glm::vec2 pos = {125.0f, 85.0f};
	std::vector<int> floats;
};

class ExampleLayer : public Cori::Core::Layer {
public:
	ExampleLayer() : Layer("Example") {


		Cori::World::SceneManager::CreateScene("Test Scene");
		BindScene("Test Scene");
		ActiveScene.GetActiveCamera().CreateOrthoCamera(0, 2560, 0, 1080, 50);
		track = Cori::Audio::Track::Create("Test");
		sound = Cori::AssetManager::Get(Sound1);

		for (int i = 0; i < 100; i++) {
			inst.floats.emplace_back(i);
		}
	}

	~ExampleLayer() {

	}
	
	virtual void OnEvent(Cori::Core::Event& event) override {

		Cori::Core::EventDispatcher dispatcher(event);


	}

	template<typename T>
	void TestAsser() {
		T def{};
		CORI_CORE_ASSERT(false, "test", def);
		//CORI_CORE_ASSERT_DEBUG(false, "lol {}", def);
	}

	virtual void OnImGuiRender(Cori::Core::GameTimer& gameTimer) override {
		ImGui::Begin("Test");

		static int a = 0;


		static bool ale = false;

		if (ale) {

			ale = false;
		}


		if (ImGui::Button("Save")) {
			Cori::FileSystem::BinaryFileManager::SaveAggregateStruct(inst, "test/inst.bin", true);
		}

		if (ImGui::Button("Load")) {
			auto result = Cori::FileSystem::BinaryFileManager::LoadAggregateStruct<Testttt>("test/inst.bin", true);
			if (result) {
				Testttt& loaded = *result;

				CORI_INFO("{}", loaded.id);
				CORI_INFO("{}", loaded.fps);
				CORI_INFO("{}", loaded.accum);
				CORI_INFO("{}", loaded.fps10);
				CORI_INFO("{}", loaded.accum10);
				CORI_INFO("{} {}", loaded.pos.x, loaded.pos.y);

				std::string fts;

				for (const auto& ft : loaded.floats) {
					fts.append(std::to_string(ft));
				}

				CORI_INFO("{}", fts);
			}
			else {
				CORI_ERROR("Failed to load aggregate struct, {}", result.error().what());
			}
		}



		ImGui::Text("FPS: %.2f", fps);
		ImGui::Text("FPS 10s avg: %.2f", fps10);


		ImGui::End();


		Cori::ImGuiPresets::ScreenModeAndResolutionDropdowns();
	}

	void OnUpdate(Cori::Core::GameTimer& gameTimer) override {
		accum++;
	}

	virtual void OnTickUpdate(Cori::Core::GameTimer& gameTimer) override {
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

	Testttt inst;
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