//#define CORI_ASYNC_LOGGING
//#define CORI_NO_FILE_LOGGING
#include <Cori.hpp>
#include <CoriEntry.hpp>
#include <format>
#define SPONZA

#define CORI_MOUSE_TEMP_DEBUG

class ExampleLayer : public Cori::Core::Layer {
public:
	ExampleLayer() : Layer("Example") {
		Cori::World::SceneManager::CreateScene("Test Scene");
		BindScene("Test Scene");
		constexpr vk::Extent2D prtExtent{ 3440, 1440 };

		auto& camera = ActiveScene.GetActiveCamera();
		camera.CreatePerspectiveCamera(60.0f, static_cast<float>(prtExtent.width) / static_cast<float>(prtExtent.height), 0.1f, 100.0f);
		camera.SetPosition3D(m_CameraPosition);
		camera.SetYawPitch(m_CameraYaw, m_CameraPitch);
		camera.RecalculateVP();

		Cori::Graphics::SceneRenderer::CreateInfo info{
			.initialPRTExtent = prtExtent,
			.PRTFormat = vk::Format::eR8G8B8A8Srgb,
			.name = "Test renderer",
			.registerPRTWithImGui = false
		};

		ActiveScene.RegisterSystem<Cori::World::Systems::RenderSync>(std::move(info));
		
		constexpr float sponzaScale = 1.0f;
		const glm::vec3 sponzaOffset{ 0.0f, 0.0f, 0.0f };

		#ifndef SPONZA
		{
			auto swordMaterial = Cori::Core::AssetManager2::Load<Cori::Graphics::Material>("assets/Sword_Material.json");
			auto swordMesh = Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>("assets/Sponza/meshes/Sponza_Mesh_015.json");
			auto entity = ActiveScene.CreateEntity("awd");
			entity.AddComponent<Cori::World::Components::Entity::Rendering>(std::move(swordMesh), std::move(swordMaterial), glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });
			auto& tc = entity.GetComponents<Cori::World::Components::Entity::Transform>();
			//tc.SetLocalScale({ 0.5f, 0.5f, 0.5f });
			tc.SetLocalScale({ sponzaScale, sponzaScale, sponzaScale });
			tc.SetLocalPosition(sponzaOffset);
		}

		{
			auto swordMaterial = Cori::Core::AssetManager2::Load<Cori::Graphics::Material>("assets/Sword_Material.json");
			auto swordMesh = Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>("assets/Sponza/meshes/Sponza_Mesh_016.json");
			auto entity = ActiveScene.CreateEntity("awd");
			entity.AddComponent<Cori::World::Components::Entity::Rendering>(std::move(swordMesh), std::move(swordMaterial), glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });
			auto& tc = entity.GetComponents<Cori::World::Components::Entity::Transform>();
			//tc.SetLocalScale({ 0.5f, 0.5f, 0.5f });
			tc.SetLocalScale({ sponzaScale, sponzaScale, sponzaScale });
			tc.SetLocalPosition(sponzaOffset);
		}


		#else
		static constexpr uint8_t sponzaMeshMaterials[] = {
			 0,  3,  1,  4,  5,  6,  7,  8,  6,  9,  7,  6, 10,  5,  7,  5,  6,  7,  6,  7,
			 6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  5,  6,  5, 11,  5,
			11,  5, 11,  5, 10,  5,  9,  8,  6, 12,  2,  5, 13,  0, 14, 15, 16, 14, 15, 14,
			16, 15, 13, 17, 18, 19, 18, 19, 18, 17, 19, 18, 17, 20, 21, 20, 21, 20, 21, 20,
			21,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1, 22, 23,  4, 23,  4,
			 5, 24,  5
		};

		static constexpr uint32_t sponzaMaterialCount = 25;


		std::vector<Cori::Core::AssetRef<Cori::Graphics::Material>> sponzaMaterials;
		sponzaMaterials.reserve(sponzaMaterialCount);

		for (uint32_t i = 0; i < sponzaMaterialCount; i++) {
			sponzaMaterials.emplace_back(Cori::Core::AssetManager2::Load<Cori::Graphics::Material>(
				std::format("assets/Sponza/materials/Sponza_Mat_{:02}.json", i).c_str()));
		}

		for (uint32_t i = 0; i < std::size(sponzaMeshMaterials); i++) {
			auto sponzaMesh = Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>(
				std::format("assets/Sponza/meshes/Sponza_Mesh_{:03}.json", i).c_str());

			auto sponzaEntity = ActiveScene.CreateEntity(std::format("Sponza_{:03}", i));
			sponzaEntity.AddComponent<Cori::World::Components::Entity::Rendering>(
				std::move(sponzaMesh), sponzaMaterials[sponzaMeshMaterials[i]], glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });

			auto& sponzaTc = sponzaEntity.GetComponents<Cori::World::Components::Entity::Transform>();
			sponzaTc.SetLocalScale({ sponzaScale, sponzaScale, sponzaScale });
			sponzaTc.SetLocalPosition(sponzaOffset);
		}

		CORI_INFO("Sponza: requested {} meshes over {} materials", std::size(sponzaMeshMaterials), sponzaMaterialCount);
		#endif
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

		ImGui::Separator();
		ImGui::Text("WASD move, Space/C up/down, Shift sprint, hold RMB to look");
		ImGui::Text("Camera: (%.2f, %.2f, %.2f)  yaw %.1f  pitch %.1f", m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, m_CameraYaw, m_CameraPitch);

		ImGui::End();

		Cori::ImGuiPresets::ScreenModeAndResolutionDropdowns();
	}

	void OnUpdate(Cori::Core::GameTimer& gameTimer) override {
		const float deltaTime = static_cast<float>(gameTimer.GetDeltaTime());
		auto& camera = ActiveScene.GetActiveCamera();

		UpdateMouseLook();
		camera.SetYawPitch(m_CameraYaw, m_CameraPitch);

		glm::vec3 direction{ 0.0f };

		if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_W)) {
			direction += camera.GetForward();
		}

		if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_S)) {
			direction -= camera.GetForward();
		}

		if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_D)) {
			direction += camera.GetRight();
		}

		if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_A)) {
			direction -= camera.GetRight();
		}

		if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_SPACE)) {
			direction += Cori::Graphics::CameraController::GetWorldUp();
		}

		if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_C)) {
			direction -= Cori::Graphics::CameraController::GetWorldUp();
		}

		if (glm::length(direction) > 0.0001f) {
			const float speed = Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_LSHIFT) ? s_MoveSpeed * s_SprintMultiplier : s_MoveSpeed;
			m_CameraPosition += glm::normalize(direction) * speed * deltaTime;
		}

		camera.SetPosition3D(m_CameraPosition);
		camera.RecalculateVP();
	}

	void OnTickUpdate(Cori::Core::GameTimer& gameTimer) override {


	}

private:
	void UpdateMouseLook() {
		const bool imGuiWantsMouse = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
		const bool canLook = m_MouseLookActive || !imGuiWantsMouse;
		const bool wantLook = Cori::Core::Input::IsMouseKeyDown(Cori::Core::CORI_MOUSEBUTTON_RIGHT) && canLook;

		if (wantLook != m_MouseLookActive) {
			m_MouseLookActive = Cori::Core::Input::SetRelativeMouseMode(wantLook) ? wantLook : false;
		}

		if (!m_MouseLookActive) {
			return;
		}

		const glm::vec2 delta = Cori::Core::Input::GetMouseDelta();

		m_CameraYaw -= delta.x * s_MouseSensitivity;
		m_CameraPitch -= delta.y * s_MouseSensitivity;
		m_CameraPitch = std::clamp(m_CameraPitch, -89.0f, 89.0f);

		#ifdef CORI_MOUSE_TEMP_DEBUG
		if (delta != glm::vec2{ 0.0f, 0.0f }) {
			CORI_DEBUG("Camera consumed delta ({}, {}) -> yaw {}, pitch {}", delta.x, delta.y, m_CameraYaw, m_CameraPitch);
		}
		#endif
	}

	static constexpr glm::vec3 s_InitialCameraPosition{ -13.0f, 0.0f, 0.7f };
	static constexpr float s_InitialCameraYaw = 0.0f;
	static constexpr float s_InitialCameraPitch = 0.0f;

	static constexpr float s_MoveSpeed = 4.0f;
	static constexpr float s_SprintMultiplier = 4.0f;
	static constexpr float s_MouseSensitivity = 0.15f;

	glm::vec3 m_CameraPosition{ s_InitialCameraPosition };
	float m_CameraYaw{ s_InitialCameraYaw };
	float m_CameraPitch{ s_InitialCameraPitch };

	bool m_MouseLookActive{ false };
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