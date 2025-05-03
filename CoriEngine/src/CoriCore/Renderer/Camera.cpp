#include "Camera.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Cori {

	void OrtoCamera::SetCameraSize(int left, int right, int bottom, int top) {
		m_ProjectionMatrix = glm::ortho((float)left, (float)right, (float)bottom, (float)top, -1.0f, 1.0f);
		m_Created = true;
		TranslateVP(m_Position);
		RotateVP(m_Angle);
	}

	void OrtoCamera::TranslateVP(const glm::vec2 newPos) {
		if CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera") return;
		m_ViewProjectionMatrix = glm::translate(m_ProjectionMatrix, glm::vec3(newPos, 0.0f));
		m_Position = newPos;
	}

	void OrtoCamera::RotateVP(const float angle) {
		if CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera") return;
		m_ViewProjectionMatrix = glm::rotate(m_ProjectionMatrix, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
		m_Angle = angle;
	}
}