#include "Camera.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Cori {

	OrtoCamera::OrtoCamera(const glm::vec4& dimensions) {
		m_ProjectionMatrix = glm::ortho(dimensions.x, dimensions.y, dimensions.z, dimensions.a, -1.0f, 1.0f);
		m_Created = true;
		TranslateVP(m_Position);
	}

	void OrtoCamera::TranslateVP(const glm::vec2 newPos) {
		if CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera") return;
		m_ViewProjectionMatrix = glm::translate(m_ProjectionMatrix, glm::vec3(newPos, 0.0f));
		m_Position = newPos;
	}

}