#pragma once
#include "Entity.hpp"
#include "../EventSystem/Event.hpp"
#include "../Renderer/Texture.hpp"

namespace Cori {

	struct NameComponent {
		std::string Name;
		NameComponent() = default;
		NameComponent(const std::string& name) : Name(name) {}
	};

	struct RenderingComponent {
		glm::vec2 Size{ 0.0f, 0.0f };
		float ZIndex{ 0.0f };
		bool Textured{ true };
		bool Visible{ true };
		RenderingComponent() = default;
		RenderingComponent(const glm::vec2& size, float zIndex = 0.0f, bool textured = true, bool visible = true)
			: Size(size), ZIndex(zIndex), Textured(textured), Visible(visible) { }
	};

	struct SpriteComponent {
		std::shared_ptr<Texture2D> Texture{ nullptr };
		UVs UV;
		SpriteComponent() = default;
		SpriteComponent(const std::shared_ptr<Texture2D>& texture, const UVs& uv = {})
			: Texture(texture), UV(uv) { }
	};

	struct PositionComponent {
		glm::vec2 Position{ 0.0f, 0.0f };
		PositionComponent() = default;
		PositionComponent(const glm::vec2& pos) : Position(pos) {}
	};

	struct TriggerComponent {
		using EventCallbackFn = std::function<void(Event&)>;

		using TriggerCallbackFn = std::function<bool(Entity&, Entity&, const EventCallbackFn)>;

		TriggerCallbackFn TriggerScript;

		TriggerComponent() = default;
		TriggerComponent(const TriggerCallbackFn& func) { TriggerScript = func; }
	};

	struct PlayerTagComponent {
		const bool placeholder{ true };
		PlayerTagComponent() = default;
	};

	struct ColliderComponent {
		glm::vec2 Size{ 0.0f, 0.0f };
		glm::vec2 Offset{ 0.0f, 0.0f };
		ColliderComponent() = default;
		ColliderComponent(const glm::vec2& size, const glm::vec2& offset = { 0.0f, 0.0f })
			: Size(size), Offset(offset) {}
	};

	struct SpawnpointComponent {
		glm::vec2 Position{ 0.0f, 0.0f };
		SpawnpointComponent() = default;
		SpawnpointComponent(const glm::vec2& pos) : Position(pos) {}
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