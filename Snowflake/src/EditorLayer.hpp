#pragma once
#include <Cori.hpp>
#include <ImGuizmo.h>

#include "AssetDragDropPayload.hpp"
#include "ContentBrowser/ContentBrowser.hpp"
#include "Inspector/ComponentInspector.hpp"

namespace Snowflake {
	class AssetPreviewLayer;

	class EditorLayer final : public Cori::Core::Layer {
	public:
		explicit EditorLayer();

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

		void DrawAssetBrowser();

		void DrawInspector();

		void UpdateViewportPicking(const ImVec2 imageOrigin, const ImVec2 region, const bool imageHovered, const bool imageClicked, const bool dragInFlight);

		void HandleViewportAssetDrop(const ImVec2 imageOrigin, const ImVec2 region);

		void ApplyAssetToEntity(const AssetDragDropPayload& payload, const entt::entity target);

		void DrawObjectGizmo(const ImVec2 imageOrigin, const ImVec2 region);

		void DrawViewGizmo(const ImVec2 imageOrigin, const ImVec2 region);

		void DrawViewportToolbar(const ImVec2 imageOrigin);

		Cori::World::Entity CreatePlaceholderEntity();

		struct PanelEntry {
			const char* name;
			bool* open;
		};

		[[nodiscard]] std::array<PanelEntry, 4> GetPanels();

		void UpdateDockNavigation();

		void FocusNeighbourDockNode(const ImGuiDir direction);

		void UpdateWindowManipulation();

		void ResizeWindow(ImGuiWindow* window, const ImVec2 delta);

		void UpdateShortcuts();

		void UpdateGizmoShortcuts();

		void CloseFocusedWindow();

		void DrawLauncher();

		void OpenLauncher();

		void CloseLauncher();

		void ActivatePanel(const PanelEntry& panel);

		void UpdateCameraCapture();

		void UpdateCamera(const float deltaTime);

		void FlushViewportResize();

		void PollPickResults();

		[[nodiscard]] bool HasHoverPickInputChanged(const ImVec2 mouse) const;

		[[nodiscard]] entt::entity ResolvePickedEntity(const Cori::Graphics::PickResult& result);

		void DrawSelectionOverlay(const ImVec2 imageOrigin);

		static constexpr const char* s_ViewportPanel{ "Viewport" };
		static constexpr const char* s_DockHostWindow{ "SnowflakeDockHost" };
		static constexpr const char* s_DockSpaceID{ "SnowflakeDockSpace" };
		static constexpr const char* s_WindowSettingsWindow{ "Window" };
		static constexpr const char* s_InspectorPanel{ ComponentInspector::s_DefaultName };

		static constexpr const char* s_SceneName{ "Test Scene" };

		const glm::vec3 s_InitialCameraPosition{ -13.0f, 0.0f, 0.7f };

		static constexpr float s_InitialCameraYaw{ 0.0f };
		static constexpr float s_InitialCameraPitch{ 0.0f };

		static constexpr float s_CameraFovY{ 90.0f };
		static constexpr float s_CameraNearPlane{ 0.1f };
		static constexpr float s_CameraFarPlane{ 100.0f };

		static constexpr float s_MoveSpeed{ 4.0f };
		static constexpr float s_SprintMultiplier{ 4.0f };
		static constexpr float s_MouseSensitivity{ 0.15f };

		static constexpr uint32_t s_ResizeSettleFrames{ 2 };

		static constexpr float s_EntitySpawnDistance{ 2.0f };

		static constexpr float s_ViewGuizmoOrbitDistance{ 8.0f };

		static constexpr float s_ViewGuizmoSnapDuration{ 0.35f };

		static constexpr float s_ViewGizmoRadius{ 80.0f };
		static constexpr float s_ViewGizmoMargin{ 12.0f };

		static constexpr float s_TranslateSnap{ 0.1f };
		static constexpr float s_RotateSnap{ 5.0f };
		static constexpr float s_ScaleSnap{ 0.1f };

		std::weak_ptr<Cori::World::Systems::RenderSync> m_RenderSync;

		vk::Extent2D m_PanelExtent{};
		vk::Extent2D m_PRTExtent{};
		uint32_t m_PanelStableFrames{ 0 };

		glm::vec3 m_CameraPosition{ s_InitialCameraPosition };
		float m_CameraYaw{ s_InitialCameraYaw };
		float m_CameraPitch{ s_InitialCameraPitch };

		std::optional<glm::vec3> m_OrbitPivot{};

		bool m_ViewSnapActive{ false };
		float m_ViewSnapElapsed{ 0.0f };
		glm::vec3 m_ViewSnapPivot{};
		float m_ViewSnapStartDistance{ 0.0f };
		float m_ViewSnapTargetDistance{ 0.0f };
		float m_ViewSnapStartYaw{ 0.0f };
		float m_ViewSnapYawDelta{ 0.0f };
		float m_ViewSnapStartPitch{ 0.0f };
		float m_ViewSnapTargetPitch{ 0.0f };

		enum class WindowManipulation : uint8_t {
			eNone,
			eMove,
			eResize
		};

		static constexpr float s_MinFloatingWindowSize{ 120.0f };
		static constexpr float s_MinDockNodeSize{ 64.0f };

		ImGuiID m_DockSpaceID{ 0 };

		WindowManipulation m_WindowManipulation{ WindowManipulation::eNone };
		ImGuiID m_ManipulatedWindow{ 0 };

		bool m_ViewportFocused{ false };
		bool m_CameraCaptureActive{ false };

		entt::entity m_SelectedEntity{ entt::null };
		entt::entity m_HoveredEntity{ entt::null };

		static constexpr uint64_t s_NoPickTicket{ UINT64_MAX };

		static constexpr uint32_t s_HoverOutlineColor{ 0xFFB86CFF };
		static constexpr uint32_t s_SelectionOutlineColor{ 0x4DA6FFFF };
		static constexpr uint32_t s_DropTargetOutlineColor{ 0x50C878FF };

		uint64_t m_ClickPickTicket{ s_NoPickTicket };
		uint64_t m_HoverAcceptFromTicket{ s_NoPickTicket };

		uint64_t m_DropPickTicket{ s_NoPickTicket };
		AssetDragDropPayload m_PendingDropPayload{};
		bool m_DropHoverActive{ false };

		ImGuizmo::OPERATION m_GizmoOperation{ ImGuizmo::TRANSLATE };
		bool m_GizmoAABBCorrection{ true };
		bool m_GizmoEnabled{ true };
		bool m_GizmoSnap{ false };

		ImVec2 m_LastHoverPickPos{ -1.0f, -1.0f };
		glm::vec3 m_LastHoverPickCameraPosition{};
		float m_LastHoverPickCameraYaw{ 0.0f };
		float m_LastHoverPickCameraPitch{ 0.0f };

		bool m_ShowWindowSettings{ false };
		bool m_RebuildLayout{ false };

		static constexpr float s_WindowSettingsWidth{ 420.0f };
		static constexpr float s_SelectionOverlayInset{ 8.0f };

		static constexpr float s_LauncherWidth{ 420.0f };
		static constexpr float s_LauncherAnchorRatio{ 0.15f };

		bool m_ShowLauncher{ false };
		bool m_LauncherJustOpened{ false };
		int32_t m_LauncherSelection{ 0 };
		char m_LauncherQuery[64]{};

		Cori::ConsolePanel m_Console;
		bool m_ShowConsole{ true };

		ContentBrowser m_Browser{};

		ComponentInspector m_Inspector{};
		bool m_ShowInspector{ true };

		uint32_t m_CreatedEntityCount{ 0 };

		Cori::World::SceneHandle m_MainScene{ nullptr };
	};
}
