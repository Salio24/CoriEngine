#pragma once

namespace Cori {
	class OrtoCamera {
	public:
		OrtoCamera() {}
		OrtoCamera(const glm::vec4& dimensions);

		void TranslateVP(const glm::vec2 newPos);

		const glm::vec2& GetPosition() const { 
			if CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera") return glm::vec2(0.0f);
			return m_Position; 
		}
		const glm::mat4& GetProjectionMatrix() const { 
			if CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera") return glm::mat4(0.0f);
			return m_ProjectionMatrix; 
		}
		const glm::mat4& GetViewProjectionMatrix() const { 
			if CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera") return glm::mat4(0.0f); 
			return m_ViewProjectionMatrix; 
		}

	private:
		bool m_Created{ false };

		glm::vec2 m_Position{ 0.0f };
		glm::mat4 m_ProjectionMatrix{ 1.0f };
		glm::mat4 m_ViewProjectionMatrix{ 1.0f };
	};
}