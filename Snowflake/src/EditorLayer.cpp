#include "EditorLayer.hpp"
#include <imgui_internal.h>
#include <format>

namespace Snowflake {
	EditorLayer::EditorLayer() : Layer("Snowflake Editor") {
		ImGuiIO& io = ImGui::GetIO();

		io.Fonts->AddFontFromFileTTF((Cori::FileSystem::PathManager::GetAliasedPath("ASSET_DIR") / "fonts/ttf/JetBrainsMono-Regular.ttf").c_str(), 16.0f);

		Cori::Core::Window& window = Cori::Core::Application::GetWindow();

		if (window.GetWindowMode() != Cori::Core::WindowMode::RESIZABLE) {
			if (const auto result = window.SetWindowMode(Cori::Core::WindowMode::RESIZABLE); !result) {
				CORI_ERROR("Failed to put the editor window into resizable mode. Error: {}", result.error().what());
			}
		}

		const vk::Extent2D initialPRTExtent{ static_cast<uint32_t>(window.GetWidth()), static_cast<uint32_t>(window.GetHeight()) };

		m_PanelExtent = initialPRTExtent;
		m_PRTExtent = initialPRTExtent;

		if (CreateTestScene(initialPRTExtent)) {
			LoadSponza();
		}
	}

	EditorLayer::~EditorLayer() {
		if (m_CameraCaptureActive) {
			Cori::Core::Input::SetRelativeMouseMode(false);
			ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
			m_CameraCaptureActive = false;
		}
	}

	bool EditorLayer::CreateTestScene(const vk::Extent2D initialPRTExtent) {
		if (const auto scene = Cori::World::SceneManager::CreateScene(s_SceneName); !scene) {
			CORI_ERROR("Failed to create scene '{}'. Error: {}", s_SceneName, scene.error().what());
			return false;
		}

		if (const auto bound = BindScene(s_SceneName); !bound) {
			CORI_ERROR("Failed to bind scene '{}' to the editor layer. Error: {}", s_SceneName, bound.error().what());
			return false;
		}

		auto& camera = ActiveScene.GetActiveCamera();
		camera.CreatePerspectiveCamera(s_CameraFovY, static_cast<float>(initialPRTExtent.width) / static_cast<float>(initialPRTExtent.height), s_CameraNearPlane, s_CameraFarPlane);
		camera.SetPosition3D(m_CameraPosition);
		camera.SetYawPitch(m_CameraYaw, m_CameraPitch);
		camera.RecalculateVP();

		Cori::Graphics::SceneRenderer::CreateInfo info{
			.initialPRTExtent = initialPRTExtent,
			.PRTFormat = vk::Format::eR8G8B8A8Srgb,
			#ifdef DEBUG_BUILD
			.name = "Snowflake viewport",
			#endif
			.registerPRTWithImGui = true
		};

		ActiveScene.RegisterSystem<Cori::World::Systems::RenderSync>(std::move(info));

		auto renderSync = ActiveScene.GetSystem<Cori::World::Systems::RenderSync>();
		if (!renderSync) {
			CORI_ERROR("Failed to retrieve the RenderSync system of '{}'. Error: {}", s_SceneName, renderSync.error().what());
			return false;
		}

		m_RenderSync = renderSync.value();

		if (const auto locked = m_RenderSync.lock()) {
			locked->Bind();
		}

		Cori::Graphics::MasterRenderer::ChangeCompositeMode(Cori::Graphics::MasterRenderer::Mode::eDockSpace);

		return true;
	}

	void EditorLayer::LoadSponza() {
		static constexpr uint8_t sponzaMeshMaterials[] = {
			 0,  3,  1,  4,  5,  6,  7,  8,  6,  9,  7,  6, 10,  5,  7,  5,  6,  7,  6,  7,
			 6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  5,  6,  5, 11,  5,
			11,  5, 11,  5, 10,  5,  9,  8,  6, 12,  2,  5, 13,  0, 14, 15, 16, 14, 15, 14,
			16, 15, 13, 17, 18, 19, 18, 19, 18, 17, 19, 18, 17, 20, 21, 20, 21, 20, 21, 20,
			21,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1, 22, 23,  4, 23,  4,
			 5, 24,  5
		};

		static constexpr uint32_t sponzaMaterialCount = 25;

		constexpr float sponzaScale = 1.0f;
		const glm::vec3 sponzaOffset{ 0.0f, 0.0f, 0.0f };

		std::vector<Cori::Core::AssetRef<Cori::Graphics::Material>> sponzaMaterials;
		sponzaMaterials.reserve(sponzaMaterialCount);

		for (uint32_t i = 0; i < sponzaMaterialCount; i++) {
			sponzaMaterials.emplace_back(Cori::Core::AssetManager2::Load<Cori::Graphics::Material>(
				std::format("assets://Sponza/materials/Sponza_Mat_{:02}.json", i).c_str()));
		}

		for (uint32_t i = 0; i < std::size(sponzaMeshMaterials); i++) {
			auto sponzaMesh = Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>(
				std::format("assets://Sponza/meshes/Sponza_Mesh_{:03}.json", i).c_str());

			auto sponzaEntity = ActiveScene.CreateEntity(std::format("Sponza_{:03}", i));
			sponzaEntity.AddComponent<Cori::World::Components::Entity::Rendering>(
				std::move(sponzaMesh), sponzaMaterials[sponzaMeshMaterials[i]], glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });

			auto& sponzaTc = sponzaEntity.GetComponents<Cori::World::Components::Entity::Transform>();
			sponzaTc.SetLocalScale({ sponzaScale, sponzaScale, sponzaScale });
			sponzaTc.SetLocalPosition(sponzaOffset);
		}

		CORI_INFO("Sponza: requested {} meshes over {} materials", std::size(sponzaMeshMaterials), sponzaMaterialCount);
	}

	void EditorLayer::OnEvent([[maybe_unused]] Cori::Core::Event& event) {}

	void EditorLayer::OnImGuiRender([[maybe_unused]] Cori::Core::GameTimer& gameTimer) {
		DrawDockSpace();
		DrawViewport();
		DrawConsole();
		DrawWindowSettings();
	}

	void EditorLayer::DrawConsole() {
		m_Console.Draw(&m_ShowConsole);
	}

	void EditorLayer::DrawDockSpace() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::Begin(s_DockHostWindow, nullptr, hostFlags);

		ImGui::PopStyleVar(3);

		DrawMenuBar();

		const ImGuiID dockSpaceID = ImGui::GetID(s_DockSpaceID);

		if (ImGui::DockBuilderGetNode(dockSpaceID) == nullptr || m_RebuildLayout) {
			m_RebuildLayout = false;
			BuildDefaultLayout(dockSpaceID, ImGui::GetContentRegionAvail());
		}

		ImGui::DockSpace(dockSpaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		ImGui::End();
	}

	void EditorLayer::BuildDefaultLayout(const ImGuiID dockSpaceID, const ImVec2 dockSpaceSize) {
		ImGui::DockBuilderRemoveNode(dockSpaceID);
		ImGui::DockBuilderAddNode(dockSpaceID, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockSpaceID, dockSpaceSize);

		ImGuiID consoleNode = 0;
		ImGuiID viewportNode = 0;
		ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Down, 0.28f, &consoleNode, &viewportNode);

		ImGui::DockBuilderDockWindow(s_ViewportPanel, viewportNode);
		ImGui::DockBuilderDockWindow(Cori::ConsolePanel::s_DefaultName, consoleNode);
		ImGui::DockBuilderFinish(dockSpaceID);
	}

	void EditorLayer::DrawMenuBar() {
		if (!ImGui::BeginMenuBar()) {
			return;
		}

		if (ImGui::BeginMenu("File")) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Assets")) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Entity")) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			if (ImGui::MenuItem(Cori::ConsolePanel::s_DefaultName, nullptr, m_ShowConsole)) {
				m_ShowConsole = !m_ShowConsole;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Reset Layout")) {
				m_RebuildLayout = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings")) {
			if (ImGui::MenuItem(s_WindowSettingsWindow)) {
				m_ShowWindowSettings = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	void EditorLayer::DrawViewport() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		const bool visible = ImGui::Begin(s_ViewportPanel, nullptr, ImGuiWindowFlags_NoCollapse);
		ImGui::PopStyleVar();

		m_ViewportHovered = ImGui::IsWindowHovered();

		if (visible) {
			const ImVec2 region = ImGui::GetContentRegionAvail();

			const vk::Extent2D panelExtent{
				static_cast<uint32_t>(std::max(region.x, 1.0f)),
				static_cast<uint32_t>(std::max(region.y, 1.0f))
			};

			if (panelExtent.width == m_PanelExtent.width && panelExtent.height == m_PanelExtent.height) {
				//if (m_PanelStableFrames < s_ResizeSettleFrames) {
				//	m_PanelStableFrames++;
				//}
			}
			else {
				m_PanelExtent = panelExtent;
				m_PanelStableFrames = 0;
			}

			const auto renderSync = m_RenderSync.lock();
			const std::optional<ImTextureID> prt = renderSync ? renderSync->GetMainPRT() : std::nullopt;

			if (prt) {
				ImGui::Image(prt.value(), region);
			}
			else {
				ImGui::TextUnformatted("Waiting for the scene render target...");
			}
		}

		ImGui::End();
	}

	void EditorLayer::DrawWindowSettings() {
		if (!m_ShowWindowSettings) {
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);

		if (ImGui::Begin(s_WindowSettingsWindow, &m_ShowWindowSettings, ImGuiWindowFlags_AlwaysAutoResize)) {
			Cori::ImGuiPresets::ScreenModeAndResolutionDropdowns();
		}

		ImGui::End();
	}

	void EditorLayer::OnUpdate(Cori::Core::GameTimer& gameTimer) {
		FlushViewportResize();
		UpdateCameraCapture();
		UpdateCamera(static_cast<float>(gameTimer.GetDeltaTime()));
	}

	void EditorLayer::OnTickUpdate([[maybe_unused]] Cori::Core::GameTimer& gameTimer) {

	}

	void EditorLayer::FlushViewportResize() {
		//if (m_PanelStableFrames < s_ResizeSettleFrames) {
		//	return;
		//}

		if (m_PanelExtent.width == 0 || m_PanelExtent.height == 0) {
			return;
		}

		if (m_PanelExtent.width == m_PRTExtent.width && m_PanelExtent.height == m_PRTExtent.height) {
			return;
		}

		m_PRTExtent = m_PanelExtent;

		if (const auto renderSync = m_RenderSync.lock()) {
			renderSync->RequestResize(m_PRTExtent);
		}

		ActiveScene.GetActiveCamera().SetAspectRatio(static_cast<float>(m_PRTExtent.width) / static_cast<float>(m_PRTExtent.height));
	}

	void EditorLayer::UpdateCameraCapture() {
		const bool middleDown = Cori::Core::Input::IsMouseKeyDown(Cori::Core::CORI_MOUSEBUTTON_MIDDLE);

		if (middleDown && !m_CameraCaptureActive) {
			if (!m_ViewportHovered) {
				return;
			}

			if (!Cori::Core::Input::SetRelativeMouseMode(true)) {
				return;
			}

			m_CameraCaptureActive = true;

			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
			return;
		}

		if (!middleDown && m_CameraCaptureActive) {
			Cori::Core::Input::SetRelativeMouseMode(false);
			m_CameraCaptureActive = false;
			ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
		}
	}

	void EditorLayer::UpdateCamera(const float deltaTime) {
		auto& camera = ActiveScene.GetActiveCamera();

		if (m_CameraCaptureActive) {
			const glm::vec2 delta = Cori::Core::Input::GetMouseDelta() * 0.25f;

			m_CameraYaw -= delta.x * s_MouseSensitivity;
			m_CameraPitch -= delta.y * s_MouseSensitivity;
			m_CameraPitch = std::clamp(m_CameraPitch, -89.0f, 89.0f);

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
		}

		camera.RecalculateVP();
	}
}
