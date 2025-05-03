#pragma once

namespace Cori {
	class OrtoCamera {
	public:
		OrtoCamera() {}

		void SetCameraSize(int left, int right, int bottom, int top);

		void TranslateVP(const glm::vec2 newPos);

		void RotateVP(const float angle);

		const glm::vec2 GetPosition() const { 
			CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera");
			return m_Position; 
		}

		const float GetRotation() const {
			CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera");
			return m_Angle;
		}

		const glm::mat4& GetProjectionMatrix() const { 
			CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera");
			return m_ProjectionMatrix; 
		}
		const glm::mat4& GetViewProjectionMatrix() const { 
			CORI_CORE_ASSERT_WARN(m_Created, "Using non initialized OrtoCamera");
			return m_ViewProjectionMatrix; 
		}

	private:
		bool m_Created{ false };

		float m_Angle{ 0.0f };
		glm::vec2 m_Position{ 0.0f };
		glm::mat4 m_ProjectionMatrix{ 1.0f };
		glm::mat4 m_ViewProjectionMatrix{ 1.0f };
	};
}