#pragma once
#include "Entity.hpp"

namespace Cori {

	struct NameComponent {
		std::string Name;
		NameComponent() = default;
		NameComponent(const std::string& name) : Name(name) {}
	};

	struct RenderingComponent {
		glm::vec2 Size{ 0.0f, 0.0f };
		float ZIndex{ 0.0f };
		bool Visible{ true };
		RenderingComponent() = default;
		RenderingComponent(const glm::vec2& size, float zIndex = 0.0f, bool visible = true)
			: Size(size), ZIndex(zIndex), Visible(visible) {}
	};

	struct PositionComponent {
		glm::vec2 Position{ 0.0f, 0.0f };
		PositionComponent() = default;
		PositionComponent(const glm::vec2& pos) : Position(pos) {}
	};

	struct TriggerComponent {
		using EventCallbackFn = std::function<bool(Entity&, Entity&)>;

		bool m_Trigger{ true };
		EventCallbackFn TriggerScript;

		TriggerComponent() = default;
		TriggerComponent(const EventCallbackFn& func) { TriggerScript = func; }
	};

	struct PlayerTagComponent {
		bool m_Trigger{ true };
		PlayerTagComponent() = default;
	};

	struct ColliderComponent {
		glm::vec2 Size{ 0.0f, 0.0f };
		glm::vec2 Offset{ 0.0f, 0.0f };
		ColliderComponent() = default;
		ColliderComponent(const glm::vec2& size, const glm::vec2& offset = { 0.0f, 0.0f })
			: Size(size), Offset(offset) {}
	};

	struct CameraContextComponent {
		static constexpr auto in_place_delete = true;

		glm::mat4 ProjectionMatrix{ 1.0f };
		glm::mat4 ViewProjectionMatrix{ 1.0f };
		glm::vec2 CameraPosition{ 0.0f };
		float CameraRotation{ 0.0f };
		float CameraZoomFactor{ 1.0f };
		CameraContextComponent() = default;
		CameraContextComponent(const glm::mat4& projectionMatrix, glm::mat4& viewProjectionMatrix, const glm::vec2& cameraPosition, float cameraRotation, float cameraZoomFactor)
			: ProjectionMatrix(projectionMatrix), ViewProjectionMatrix(viewProjectionMatrix), CameraPosition(cameraPosition), CameraRotation(cameraRotation), CameraZoomFactor(cameraZoomFactor) {}
	};
}