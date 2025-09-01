#define CORI_ASYNC_LOGGING
#define CORI_NO_FILE_LOGGING
#include <Cori.hpp>
#include <CoriEntry.hpp>

CORI_DECLARE_TAG(Test);

namespace Cori {
	namespace Texture2Ds {
		inline const Texture2DDescriptor AtlasTexture{
			"Test AtlasTexture",
			"assets/engine/textures/testTileset32.png"
		};

		inline const Texture2DDescriptor UVSample{
			"Test UV text",
			"assets/engine/textures/uv_sample.png"
		};
	}

	namespace SpriteAtlases {
		inline const SpriteAtlasDescriptor Atlas{
			"Test Atlas",
			"assets/engine/textures/testTileset32.png",
			{32, 32}
		};

		inline const SpriteAtlasDescriptor UVsss{
			"Test UV Atlas",
			"assets/engine/textures/uv_sample.png",
			{128, 128}
		};

	}

	namespace Sounds {
		inline const SoundDescriptor TestSound1{
			"Test1",
			"assets/engine/sounds/coin.wav"
		};

		inline const SoundDescriptor TestSound2{
			"Test2",
			"assets/engine/sounds/power_up.wav"
		};

		inline const SoundDescriptor TestMusic{
			"TestMusic",
			"assets/engine/sounds/Try and Solve This Loop.wav"
		};
	}
}

class CustomEvent : public Cori::Event {
public:
	CustomEvent(const std::string& somedata) : m_Data(somedata) {}

	std::string ToString() const override {
		return "UDE";
	}

	std::string& GetData() {
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
		Cori::API::SetViewport(0, 0, Cori::Application::GetWindow().GetWidth(), Cori::Application::GetWindow().GetHeight());

		Cori::SceneManager::CreateScene("Test Scene");
		BindScene("Test Scene");
		ActiveScene.GetActiveCamera().CreateOrthoCamera(0, 2560, 0, 1080, -50, 0);
		track = Cori::Audio::Track::Create("Test");
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

	template<typename T>
	void TestAsser() {
		T def{};
		CORI_CORE_ASSERT(false, "test", def);
		//CORI_CORE_ASSERT_DEBUG(false, "lol {}", def);
	}

	virtual void OnImGuiRender(const double deltaTime) override {
		ImGui::Begin("Test");

		static int a = 0;

		static Cori::Entity ent;

		static bool ale = false;

		if (ale) {

			ale = false;
		}

		if (ImGui::Button("Trace")) {
			CORI_PROFILE_REQUEST_NEXT_FRAME();
			ale = true;
		}

		if (ImGui::Button("Create tree")) {
			auto atlas1 = Cori::AssetManager::GetSpriteAtlas(Cori::SpriteAtlases::UVsss);
			std::shared_ptr<Cori::SpriteAtlas> atlas;
			if (atlas1) {
				atlas = atlas1.value();
			}

			auto text = atlas->GetTexture();

			auto uvs = atlas->GetSpriteUVsAtIndex(0);
			auto uvs1 = atlas->GetSpriteUVsAtIndex(7);
			auto uvs2 = atlas->GetSpriteUVsAtIndex(56);
			auto uvs3 = atlas->GetSpriteUVsAtIndex(63);
			auto uvs4 = atlas->GetSpriteUVsAtIndex(4);

			auto player = ActiveScene.CreateEntity("Player", Test);
			auto& transform = player.GetComponents<Cori::Components::Entity::Transform>();
			transform.SetLocalDepth(2);
			transform.SetLocalPosition({ 100.0f, 100.0f });
			auto& renderer = player.AddComponent<Cori::Components::Entity::QuadRenderer>(glm::vec2(50.0f), text, uvs);
			//renderer.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
			ActiveScene.AddEntityToCache(player, "player"_hs32);

			auto player_sprite = ActiveScene.CreateEntity("Sprite", Test);
			auto& transform1 = player_sprite.GetComponents<Cori::Components::Entity::Transform>();
			transform1.SetLocalPosition({0.0f, 100.0f});
			auto& renderer1 = player_sprite.AddComponent<Cori::Components::Entity::QuadRenderer>(glm::vec2(50.0f), text, uvs1);
			//renderer1.SetColor({0.5f, 1.0f, 1.0f, 1.0f});
			player_sprite.SetParent(player);

			auto weapon_root = ActiveScene.CreateEntity("Weapon", Test);
			auto& transform2 = weapon_root.GetComponents<Cori::Components::Entity::Transform>();
			transform2.SetLocalPosition({0.0f, 200.0f});
			auto& renderer2 = weapon_root.AddComponent<Cori::Components::Entity::QuadRenderer>(glm::vec2(50.0f), text, uvs2);
			//renderer2.SetColor({1.0f, 0.5f, 1.0f, 1.0f});
			weapon_root.SetParent(player);
			ActiveScene.AddEntityToCache(weapon_root, "weapon"_hs32);

			auto gun_sprite = ActiveScene.CreateEntity("GunSprite", Test);
			auto& transform3 = gun_sprite.GetComponents<Cori::Components::Entity::Transform>();
			transform3.SetLocalPosition({0.0f, 300.0f});
			auto& renderer3 = gun_sprite.AddComponent<Cori::Components::Entity::QuadRenderer>(glm::vec2(50.0f), text, uvs3);
			//renderer3.SetColor({1.0f, 1.0f, 0.5f, 1.0f});
			gun_sprite.SetParent(weapon_root);

			auto muzzle_flash = ActiveScene.CreateEntity("MuzzleFlash", Test);
			auto& transform4 = muzzle_flash.GetComponents<Cori::Components::Entity::Transform>();
			transform4.SetLocalPosition({0.0f, 400.0f});
			auto& renderer4 = muzzle_flash.AddComponent<Cori::Components::Entity::QuadRenderer>(glm::vec2(50.0f), text, uvs4);
			//renderer4.SetColor({1.0f, 0.0f, 1.0f, 1.0f});
			muzzle_flash.SetParent(weapon_root);
		}

		if (ImGui::Button("Draw hier")) {
			auto player = ActiveScene.GetEntityFromCache("player"_hs32);
			if (player) {
				player.value().PrintHierarchy();
			} else {
				player.error().ignore();
			}
		}

		auto player = ActiveScene.GetEntityFromCache("player"_hs32);
		if (player) {
			auto& transform = player.value().GetComponents<Cori::Components::Entity::Transform>();
			auto curpos = transform.GetLocalPosition();
			auto currot = transform.GetLocalRotation();
			auto curscale = transform.GetLocalScale();
			if (ImGui::DragFloat("Player Pos", &curpos.x, 1, 1, 1000, "%f", ImGuiSliderFlags_AlwaysClamp)) {
				transform.SetLocalPosition(curpos);
			}
			if (ImGui::DragFloat("Player Rot", &currot, 1.0f, -720.0f, 720.0f, "%f", ImGuiSliderFlags_AlwaysClamp)) {
				transform.SetLocalRotation(currot);
			}
			if (ImGui::DragFloat("Player Scale", &curscale.x, 0.1f, -3.0f, 3, "%f", ImGuiSliderFlags_AlwaysClamp)) {
				transform.SetLocalScale({curscale.x, 1.0f});
			}
		} else {
			player.error().ignore();
		}

		auto weapon = ActiveScene.GetEntityFromCache("weapon"_hs32);
		if (weapon) {
			auto& transform = weapon.value().GetComponents<Cori::Components::Entity::Transform>();
			auto curpos = transform.GetLocalPosition();
			auto currot = transform.GetLocalRotation();
			auto curscale = transform.GetLocalScale();
			if (ImGui::DragFloat("weapon Pos", &curpos.x, 1, 1, 1000, "%f", ImGuiSliderFlags_AlwaysClamp)) {
				transform.SetLocalPosition(curpos);
			}
			if (ImGui::DragFloat("weapon Rot", &currot, 1.0f, -720.0f, 720.0f, "%f", ImGuiSliderFlags_AlwaysClamp)) {
				transform.SetLocalRotation(currot);
			}
			if (ImGui::DragFloat("weapon Scale", &curscale.x, 0.1f, -3.0f, 3, "%f", ImGuiSliderFlags_AlwaysClamp)) {
				transform.SetLocalScale({curscale.x, 1.0f});
			}
		} else {
			weapon.error().ignore();
		}

		if (ImGui::Button("Play")) {
			track->Play();
		}
		if (ImGui::Button("Test1")) {
			track->SetSound(Cori::AssetManager::GetSound(Cori::Sounds::TestSound1));
		}

		if (ImGui::Button("Test2")) {
			track->SetSound(Cori::AssetManager::GetSound(Cori::Sounds::TestSound2));
		}

		if (ImGui::Button("TestMusic")) {
			track->SetSound(Cori::AssetManager::GetSound(Cori::Sounds::TestMusic));
		}

		if (ImGui::Button("Stop")) {
			track->Stop(300);
		}

		if (ImGui::Button("callback")) {


		}






		ImGui::Text("FPS: %.2f", fps);
		ImGui::Text("FPS 10s avg: %.2f", fps10);


		ImGui::End();
	}

	void OnUpdate(const Cori::GameTimer& gameTimer) override {
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

	float fps;
	float fps10;
	uint32_t accum{ 0 };
	uint32_t accum10{ 0 };

};

class Sandbox : public Cori::Application {
public:
	Sandbox(): Application("sandbox") {
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