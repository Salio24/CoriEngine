#pragma once
#include <Cori.hpp>

namespace Snowflake {
	class EditorLayer final : public Cori::Core::Layer {
	public:
		EditorLayer();

		~EditorLayer() override;

		void OnEvent(Cori::Core::Event& event) override;

		void OnUpdate(Cori::Core::GameTimer& gameTimer) override;

		void OnTickUpdate(Cori::Core::GameTimer& gameTimer) override;

		void OnImGuiRender(Cori::Core::GameTimer& gameTimer) override;

	private:
		[[nodiscard]] bool CreateTestScene(const vk::Extent2D initialPRTExtent);

		void LoadSponza();

		void DrawDockSpace();

		void BuildDefaultLayout(const ImGuiID dockSpaceID, const ImVec2 dockSpaceSize);

		void DrawMenuBar();

		void DrawViewport();

		void DrawWindowSettings();

		void DrawConsole();

		void UpdateCameraCapture();

		void UpdateCamera(const float deltaTime);

		void FlushViewportResize();

		static constexpr const char* s_ViewportPanel{ "Viewport" };
		static constexpr const char* s_DockHostWindow{ "SnowflakeDockHost" };
		static constexpr const char* s_DockSpaceID{ "SnowflakeDockSpace" };
		static constexpr const char* s_WindowSettingsWindow{ "Window" };

		static constexpr const char* s_SceneName{ "Test Scene" };

		static constexpr glm::vec3 s_InitialCameraPosition{ -13.0f, 0.0f, 0.7f };
		static constexpr float s_InitialCameraYaw{ 0.0f };
		static constexpr float s_InitialCameraPitch{ 0.0f };

		static constexpr float s_CameraFovY{ 90.0f };
		static constexpr float s_CameraNearPlane{ 0.1f };
		static constexpr float s_CameraFarPlane{ 100.0f };

		static constexpr float s_MoveSpeed{ 4.0f };
		static constexpr float s_SprintMultiplier{ 4.0f };
		static constexpr float s_MouseSensitivity{ 0.15f };

		static constexpr uint32_t s_ResizeSettleFrames{ 2 };

		std::weak_ptr<Cori::World::Systems::RenderSync> m_RenderSync;

		vk::Extent2D m_PanelExtent{};
		vk::Extent2D m_PRTExtent{};
		uint32_t m_PanelStableFrames{ 0 };

		glm::vec3 m_CameraPosition{ s_InitialCameraPosition };
		float m_CameraYaw{ s_InitialCameraYaw };
		float m_CameraPitch{ s_InitialCameraPitch };

		bool m_ViewportHovered{ false };
		bool m_CameraCaptureActive{ false };

		bool m_ShowWindowSettings{ false };
		bool m_RebuildLayout{ false };

		Cori::ConsolePanel m_Console;
		bool m_ShowConsole{ true };
	};
}
