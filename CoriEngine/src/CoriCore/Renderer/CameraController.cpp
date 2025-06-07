#include "CameraController.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Cori {
	void CameraController::CreateOrthoCamera(int left, int right, int bottom, int top, float zNear /*= -1.0f*/, float zFar /*= 1.0f*/) {
		m_CurrentCameraComponent->ProjectionMatrix = glm::ortho((float)left, (float)right, (float)bottom, (float)top, zNear, zFar);
		RecalculateVP();
	}

	void CameraController::SetPosition(const glm::vec2 newPos) {
		m_CurrentCameraComponent->CameraPosition = newPos;
		RecalculateVP();
	}

	void CameraController::SetRotation(const float angle) {
		m_CurrentCameraComponent->CameraRotation = angle;
		RecalculateVP();
	}

	void CameraController::SetZoomLevel(const float factor) {
		CORI_CORE_ASSERT_WARN(factor > 0.0f, "Are you sure you want zoom factor to be <= 0 ? ");
		m_CurrentCameraComponent->CameraZoomFactor = factor;
		RecalculateVP();
	}

	void CameraController::RecalculateVP() {
		glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-m_CurrentCameraComponent->CameraPosition, 0.0f)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_CurrentCameraComponent->CameraRotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
			glm::scale(glm::mat4(1.0f), glm::vec3(m_CurrentCameraComponent->CameraZoomFactor, m_CurrentCameraComponent->CameraZoomFactor, 1.0f));

		m_CurrentCameraComponent->ViewProjectionMatrix = m_CurrentCameraComponent->ProjectionMatrix * view;

	}


}