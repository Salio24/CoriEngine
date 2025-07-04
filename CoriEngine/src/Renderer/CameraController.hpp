#pragma once
#include "SceneSystem/Components.hpp"

namespace Cori {
	class CameraController {
	public:
		CameraController() = default;
		~CameraController() = default;

		void CreateOrthoCamera(float left, float right, float bottom, float top, float zNear = -10.0f, float zFar = 0.0f);

		void SetPosition(const glm::vec2 newPos);

		void SetRotation(const float angle);

		void SetZoomLevel(const float factor);

		glm::vec2 GetPosition() const;

		float GetRotation() const;

		float GetZoomLevel() const;

		void RecalculateVP();

		void BindCameraComponent(Components::Scene::Camera* camera) {
			m_CurrentCameraComponent = camera;
		}

	private:

		Components::Scene::Camera* m_CurrentCameraComponent{ nullptr };

	};
}
