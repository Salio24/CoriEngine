#pragma once
#include "Utility/AABB.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			/**
			 * @brief Components designed to be used with scenes.
			 */
			namespace Scene {
				/**
				 * @brief Which set of camera parameters the CameraController drives, and which projection it builds.
				 */
				enum class CameraMode : uint8_t {
					eOrthographic2D,
					ePerspective3D
				};

				/**
				 * @brief A Scene context component with all the graphical camera data.
				 */
				struct Camera {
					static constexpr auto in_place_delete = true;

					CameraMode m_Mode{ CameraMode::eOrthographic2D };

					glm::mat4 m_ProjectionMatrix{ 1.0f };
					glm::mat4 m_ViewMatrix{ 1.0f };
					glm::mat4 m_ViewProjectionMatrix{ 1.0f };

					//2D state, only meaningful in eOrthographic2D.
					glm::vec2 m_CameraPosition{ 0.0f };
					float m_CameraRotation{ 0.0f };
					glm::vec2 m_CameraScale{ 1.0f };
					glm::vec2 m_InitialCameraMinBound{ 0.0f };
					glm::vec2 m_InitialCameraMaxBound{ 0.0f };
					Utility::AABB m_CameraBounds{};
					glm::vec2 m_CameraSize{ 0.0f };

					//3D state, only meaningful in ePerspective3D. The world is Z up, so yaw turns around Z and pitch tilts away from the XY plane, both in degrees.
					glm::vec3 m_CameraPosition3D{ 0.0f };
					float m_Yaw{ 0.0f };
					float m_Pitch{ 0.0f };
					float m_FovY{ 60.0f };
					float m_AspectRatio{ 16.0f / 9.0f };
					float m_NearPlane{ 0.1f };
					float m_FarPlane{ 1000.0f };
					Camera() = default;
					Camera(const glm::mat4& projectionMatrix, const glm::mat4& viewProjectionMatrix, const glm::vec2& cameraPosition, const float cameraRotation, const glm::vec2 cameraScale)
						: m_ProjectionMatrix(projectionMatrix), m_ViewProjectionMatrix(viewProjectionMatrix), m_CameraPosition(cameraPosition), m_CameraRotation(cameraRotation), m_CameraScale(cameraScale) {
					}
				};
			}
		}
	}
}
