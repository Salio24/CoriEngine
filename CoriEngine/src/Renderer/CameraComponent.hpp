#pragma once

namespace Cori {
	namespace Components {
		namespace Scene {
			struct Camera {
				static constexpr auto in_place_delete = true;

				glm::mat4 m_ProjectionMatrix{ 1.0f };
				glm::mat4 m_ViewProjectionMatrix{ 1.0f };
				glm::vec2 m_CameraPosition{ 0.0f };
				float m_CameraRotation{ 0.0f };
				float m_CameraZoomFactor{ 1.0f };
				glm::vec2 m_InitialCameraMinBound{ 0.0f };
				glm::vec2 m_InitialCameraMaxBound{ 0.0f };
				glm::vec2 m_CameraMaxBound{ 0.0f };
				glm::vec2 m_CameraMinBound{ 0.0f };
				Camera() = default;
				Camera(const glm::mat4& projectionMatrix, glm::mat4& viewProjectionMatrix, const glm::vec2& cameraPosition, float cameraRotation, float cameraZoomFactor)
					: m_ProjectionMatrix(projectionMatrix), m_ViewProjectionMatrix(viewProjectionMatrix), m_CameraPosition(cameraPosition), m_CameraRotation(cameraRotation), m_CameraZoomFactor(cameraZoomFactor) {
				}
			};
		}
	}
}