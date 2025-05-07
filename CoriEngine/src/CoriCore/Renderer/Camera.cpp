// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Camera.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Cori {
	// rename to ortho
	void OrthoCamera::SetCameraSize(int left, int right, int bottom, int top) {
		m_ProjectionMatrix = glm::ortho((float)left, (float)right, (float)bottom, (float)top, -1.0f, 1.0f);
		m_Created = true;
		TranslateVP(m_Position);
		RotateVP(m_Angle);
	}

	void OrthoCamera::TranslateVP(const glm::vec2 newPos) {
		m_Position = newPos;
		RecalculateVP();
	}

	void OrthoCamera::RotateVP(const float angle) {
		m_Angle = angle;
		RecalculateVP();
	}

	void OrthoCamera::ZoomVP(const float factor) {
		if (factor <= 0.0f) { CORI_CORE_WARN("Are you sure you want zoom factor to be 0?"); }
		m_ZoomFactor = factor;
		RecalculateVP();
	}

	void OrthoCamera::RecalculateVP() {
		if CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrthoCamera") return;

		glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-m_Position, 0.0f)) * 
							glm::rotate(glm::mat4(1.0f), glm::radians(m_Angle), glm::vec3(0.0f, 0.0f, 1.0f)) * 
							glm::scale(glm::mat4(1.0f), glm::vec3(m_ZoomFactor, m_ZoomFactor, 1.0f));

		m_ViewProjectionMatrix = m_ProjectionMatrix * view;
	}
}