#include "CameraController.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Cori {
	namespace Graphics {
		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::CreateOrthoCamera(const float left, float right, const float bottom, const float top, const uint8_t depth /*=50*/) {
			m_CurrentCameraComponent->m_Mode = CameraMode::eOrthographic2D;
			m_CurrentCameraComponent->m_ProjectionMatrix = glm::ortho(left, right, bottom, top, static_cast<float>(-depth), 0.0f);
			m_CurrentCameraComponent->m_ViewProjectionMatrix = m_CurrentCameraComponent->m_ProjectionMatrix;
			m_CurrentCameraComponent->m_InitialCameraMinBound = { left, bottom };
			m_CurrentCameraComponent->m_InitialCameraMaxBound = { right, top };
			m_CurrentCameraComponent->m_CameraSize = { std::abs(right - left), std::abs(top - bottom) };

			RecalculateVP();

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Camera }, "Created orthographic camera with properties - (left: {}, right: {}, bottom: {}, top: {}, depth: {})", left, right, bottom, top, depth);
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::CreatePerspectiveCamera(const float fovY, const float aspectRatio, const float nearPlane /*=0.1f*/, const float farPlane /*=1000.0f*/) {
			m_CurrentCameraComponent->m_Mode = CameraMode::ePerspective3D;
			m_CurrentCameraComponent->m_FovY = fovY;
			m_CurrentCameraComponent->m_AspectRatio = aspectRatio;
			m_CurrentCameraComponent->m_NearPlane = nearPlane;
			m_CurrentCameraComponent->m_FarPlane = farPlane;

			RebuildPerspectiveProjection();
			RecalculateVP();

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Camera }, "Created perspective camera with properties - (fovY: {}, aspect ratio: {}, near: {}, far: {})", fovY, aspectRatio, nearPlane, farPlane);
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::RebuildPerspectiveProjection() {
			m_CurrentCameraComponent->m_ProjectionMatrix = glm::perspective(glm::radians(m_CurrentCameraComponent->m_FovY), m_CurrentCameraComponent->m_AspectRatio, m_CurrentCameraComponent->m_FarPlane, m_CurrentCameraComponent->m_NearPlane);
			m_CurrentCameraComponent->m_ProjectionMatrix[1][1] *= -1.0f;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::SetPosition(const glm::vec2 newPos) {
			m_CurrentCameraComponent->m_CameraPosition = newPos;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::SetPosition3D(const glm::vec3 newPos) {
			m_CurrentCameraComponent->m_CameraPosition3D = newPos;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::SetYawPitch(const float yaw, const float pitch) {
			m_CurrentCameraComponent->m_Yaw = yaw;
			m_CurrentCameraComponent->m_Pitch = std::clamp(pitch, -89.0f, 89.0f);
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::SetFovY(const float fovY) {
			m_CurrentCameraComponent->m_FovY = fovY;
			RebuildPerspectiveProjection();
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::SetAspectRatio(const float aspectRatio) {
			m_CurrentCameraComponent->m_AspectRatio = aspectRatio;
			RebuildPerspectiveProjection();
		}

		glm::vec3 CameraController::GetPosition3D() const {
			return m_CurrentCameraComponent->m_CameraPosition3D;
		}

		float CameraController::GetYaw() const {
			return m_CurrentCameraComponent->m_Yaw;
		}

		float CameraController::GetPitch() const {
			return m_CurrentCameraComponent->m_Pitch;
		}

		glm::vec3 CameraController::GetForward() const {
			const float yaw = glm::radians(m_CurrentCameraComponent->m_Yaw);
			const float pitch = glm::radians(m_CurrentCameraComponent->m_Pitch);
			const float cosPitch = std::cos(pitch);

			return glm::normalize(glm::vec3{ cosPitch * std::cos(yaw), cosPitch * std::sin(yaw), std::sin(pitch) });
		}

		glm::vec3 CameraController::GetRight() const {
			return glm::normalize(glm::cross(GetForward(), GetWorldUp()));
		}

		CameraMode CameraController::GetMode() const {
			return m_CurrentCameraComponent->m_Mode;
		}

		const glm::mat4& CameraController::GetViewMatrix() const {
			return m_CurrentCameraComponent->m_ViewMatrix;
		}

		const glm::mat4& CameraController::GetProjectionMatrix() const {
			return m_CurrentCameraComponent->m_ProjectionMatrix;
		}

		const glm::mat4& CameraController::GetViewProjectionMatrix() const {
			return m_CurrentCameraComponent->m_ViewProjectionMatrix;
		}

		glm::vec2 CameraController::GetPosition() const {
			return m_CurrentCameraComponent->m_CameraPosition;
		}

		float CameraController::GetRotation() const {
			return m_CurrentCameraComponent->m_CameraRotation;
		}

		glm::vec2 CameraController::GetScale() const {
			return m_CurrentCameraComponent->m_CameraScale;
		}

		glm::vec2 CameraController::GetSize() const {
			return m_CurrentCameraComponent->m_CameraSize;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::SetRotation(const float angle) {
			m_CurrentCameraComponent->m_CameraRotation = angle;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::SetScale(const glm::vec2 factor) {
			m_CurrentCameraComponent->m_CameraScale = factor;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void CameraController::RecalculateVP() {
			if (m_CurrentCameraComponent->m_Mode == CameraMode::ePerspective3D) {
				const glm::vec3 position = m_CurrentCameraComponent->m_CameraPosition3D;

				m_CurrentCameraComponent->m_ViewMatrix = glm::lookAt(position, position + GetForward(), GetWorldUp());
				m_CurrentCameraComponent->m_ViewProjectionMatrix = m_CurrentCameraComponent->m_ProjectionMatrix * m_CurrentCameraComponent->m_ViewMatrix;

				return;
			}

			glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(m_CurrentCameraComponent->m_CameraSize / 2.0f, 0.0f)) *
				glm::rotate(glm::mat4(1.0f), glm::radians(m_CurrentCameraComponent->m_CameraRotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
				glm::scale(glm::mat4(1.0f), glm::vec3(m_CurrentCameraComponent->m_CameraScale, 1.0f)) *
					glm::translate(glm::mat4(1.0f), glm::vec3(-(m_CurrentCameraComponent->m_CameraSize / 2.0f + m_CurrentCameraComponent->m_CameraPosition), 0.0f));

			m_CurrentCameraComponent->m_ViewMatrix = view;
			m_CurrentCameraComponent->m_ViewProjectionMatrix = m_CurrentCameraComponent->m_ProjectionMatrix * view;

			glm::mat3 translation;
			translation[0] = glm::vec3(view[0].x, view[0].y, view[0].w);
			translation[1] = glm::vec3(view[1].x, view[1].y, view[1].w);
			translation[2] = glm::vec3(view[3].x, view[3].y, view[3].w);
			translation = glm::inverse(translation) * glm::translate(glm::mat3(1.0f), glm::vec2(m_CurrentCameraComponent->m_CameraSize / 2.0f));

			m_CurrentCameraComponent->m_CameraBounds = Utility::CalculateAABB(translation, m_CurrentCameraComponent->m_CameraSize / 2.0f);
		}
	}
}