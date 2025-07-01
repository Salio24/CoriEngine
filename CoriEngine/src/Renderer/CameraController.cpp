#include "CameraController.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Cori {
	void CameraController::CreateOrthoCamera(float left, float right, float bottom, float top, float zNear /*= -10.0f*/, float zFar /*= 0.0f*/) {
		m_CurrentCameraComponent->m_ProjectionMatrix = glm::ortho(left, right, bottom, top, zNear, zFar);
		m_CurrentCameraComponent->m_ViewProjectionMatrix = m_CurrentCameraComponent->m_ProjectionMatrix;
		m_CurrentCameraComponent->m_InitialCameraMinBound = { left, bottom };
		m_CurrentCameraComponent->m_InitialCameraMaxBound = { right, top };
		m_CurrentCameraComponent->m_CameraMinBound = { left, bottom };
		m_CurrentCameraComponent->m_CameraMaxBound = { right, top };

		CORI_CORE_DEBUG("Created orthographic camera with properties - (left: {}, right: {}, bottom: {}, top: {}, zNear: {}, zFar: {})", left, right, bottom, top, zNear, zFar);
	}

	void CameraController::SetPosition(const glm::vec2 newPos) {
		m_CurrentCameraComponent->m_CameraPosition = newPos;
		//RecalculateVP();
	}

	void CameraController::SetRotation(const float angle) {
		m_CurrentCameraComponent->m_CameraRotation = angle;
		//RecalculateVP();
	}

	void CameraController::SetZoomLevel(const float factor) {
		CORI_CORE_ASSERT_WARN(factor > 0.0f, "Are you sure you want zoom factor to be <= 0 ? ");
		m_CurrentCameraComponent->m_CameraZoomFactor = factor;
		//RecalculateVP();
	}

	void CameraController::RecalculateVP() {
		glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-m_CurrentCameraComponent->m_CameraPosition, 0.0f)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_CurrentCameraComponent->m_CameraRotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
			glm::scale(glm::mat4(1.0f), glm::vec3(m_CurrentCameraComponent->m_CameraZoomFactor, m_CurrentCameraComponent->m_CameraZoomFactor, 1.0f));

		m_CurrentCameraComponent->m_ViewProjectionMatrix = m_CurrentCameraComponent->m_ProjectionMatrix * view;

		// idk maybe add rotation support?
		// but rotation looks awful with pixelart
		m_CurrentCameraComponent->m_CameraMinBound = { (m_CurrentCameraComponent->m_InitialCameraMinBound.x + m_CurrentCameraComponent->m_CameraPosition.x) * m_CurrentCameraComponent->m_CameraZoomFactor, (m_CurrentCameraComponent->m_InitialCameraMinBound.y + m_CurrentCameraComponent->m_CameraPosition.y) * m_CurrentCameraComponent->m_CameraZoomFactor };
		m_CurrentCameraComponent->m_CameraMaxBound = { (m_CurrentCameraComponent->m_InitialCameraMaxBound.x + m_CurrentCameraComponent->m_CameraPosition.x) * m_CurrentCameraComponent->m_CameraZoomFactor, (m_CurrentCameraComponent->m_InitialCameraMaxBound.y + m_CurrentCameraComponent->m_CameraPosition.y) * m_CurrentCameraComponent->m_CameraZoomFactor };
	}


}