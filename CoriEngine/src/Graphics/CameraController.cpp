#include "CameraController.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Cori {
	// ReSharper disable once CppMemberFunctionMayBeConst
	void CameraController::CreateOrthoCamera(float left, float right, float bottom, float top, float zNear /*= -50.0f*/, float zFar /*= 0.0f*/) {
		m_CurrentCameraComponent->m_ProjectionMatrix = glm::ortho(left, right, bottom, top, zNear, zFar);
		m_CurrentCameraComponent->m_ViewProjectionMatrix = m_CurrentCameraComponent->m_ProjectionMatrix;
		m_CurrentCameraComponent->m_InitialCameraMinBound = { left, bottom };
		m_CurrentCameraComponent->m_InitialCameraMaxBound = { right, top };
		m_CurrentCameraComponent->m_CameraSize = { std::abs(right - left), std::abs(top - bottom) };

		CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Camera }, "Created orthographic camera with properties - (left: {}, right: {}, bottom: {}, top: {}, zNear: {}, zFar: {})", left, right, bottom, top, zNear, zFar);
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void CameraController::SetPosition(const glm::vec2 newPos) {
		m_CurrentCameraComponent->m_CameraPosition = newPos;
	}

	glm::vec2 CameraController::GetPosition() const {
		return m_CurrentCameraComponent->m_CameraPosition;
	}

	float CameraController::GetRotation() const {
		return m_CurrentCameraComponent->m_CameraRotation;
	}

	float CameraController::GetZoomLevel() const {
		return m_CurrentCameraComponent->m_CameraZoomFactor;
	}

	glm::vec2 CameraController::GetSize() const {
		return m_CurrentCameraComponent->m_CameraSize;
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void CameraController::SetRotation(const float angle) {
		m_CurrentCameraComponent->m_CameraRotation = angle;
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void CameraController::SetZoomLevel(const float factor) {
		m_CurrentCameraComponent->m_CameraZoomFactor = factor;
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void CameraController::RecalculateVP() {
		glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(m_CurrentCameraComponent->m_CameraSize / 2.0f, 0.0f)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_CurrentCameraComponent->m_CameraRotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
			glm::scale(glm::mat4(1.0f), glm::vec3(m_CurrentCameraComponent->m_CameraZoomFactor, m_CurrentCameraComponent->m_CameraZoomFactor, 1.0f)) *
				glm::translate(glm::mat4(1.0f), glm::vec3(-(m_CurrentCameraComponent->m_CameraSize / 2.0f + m_CurrentCameraComponent->m_CameraPosition), 0.0f));

		m_CurrentCameraComponent->m_ViewProjectionMatrix = m_CurrentCameraComponent->m_ProjectionMatrix * view;

		glm::mat3 translation;
		translation[0] = glm::vec3(view[0].x, view[0].y, view[0].w);
		translation[1] = glm::vec3(view[1].x, view[1].y, view[1].w);
		translation[2] = glm::vec3(view[3].x, view[3].y, view[3].w);
		translation = glm::inverse(translation) * glm::translate(glm::mat3(1.0f), glm::vec2(m_CurrentCameraComponent->m_CameraSize / 2.0f));

		m_CurrentCameraComponent->m_CameraBounds = Utility::CalculateAABB(translation, m_CurrentCameraComponent->m_CameraSize / 2.0f);
	}


}