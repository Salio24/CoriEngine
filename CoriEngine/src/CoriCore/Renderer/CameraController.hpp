#pragma once
#include "../ECS/Components.hpp"

namespace Cori {
	class CameraController {
	public:
		CameraController() = default;
		~CameraController() = default;

		void CreateOrthoCamera(float left, float right, float bottom, float top, float zNear = -1.0f, float zFar = 1.0f);

		void SetPosition(const glm::vec2 newPos);

		void SetRotation(const float angle);

		void SetZoomLevel(const float factor);

		void RecalculateVP();

		void BindCameraComponent(CameraContextComponent* camera) {
			m_CurrentCameraComponent = camera;
		}

	private:

		CameraContextComponent* m_CurrentCameraComponent{ nullptr };

	};
}
