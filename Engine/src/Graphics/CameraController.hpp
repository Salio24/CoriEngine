#pragma once
#include "CameraComponent.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace World {
		class Scene;
	}

	namespace Graphics {
		using CameraMode = World::Components::Scene::CameraMode;

		/**
		 * @brief A class that is used to manipulate Camera of the Scene. Each Scene has one of those.
		 * @details The camera runs in one of two modes. eOrthographic2D drives position/rotation/scale in the XY plane, ePerspective3D drives a position and a yaw/pitch pair in a Z up world. The setters of the mode that isn't active still write their state, they just don't affect the matrices until the camera is recreated in that mode.
		 * @note I left 2D mode be, even tho it is unusable currently, might need it later.
		 */
		class CameraController {
		public:
			~CameraController() = default;

			/**
			 * @brief Creates or recreate an orthographic camera. Switches the camera to eOrthographic2D.
			 * @param left Left camera border.
			 * @param right Right camera border.
			 * @param bottom Bottom camera border.
			 * @param top Top camera border.
			 * @param depth Depth of the camera. All object that have a depth higher that this value will not be rendered.
			 */
			void CreateOrthoCamera(const float left, float right, const float bottom, const float top, const uint8_t depth = 50);

			/**
			 * @brief Creates or recreates a perspective camera. Switches the camera to ePerspective3D.
			 * @param fovY Vertical field of view, in degrees.
			 * @param aspectRatio Width over height of the target the camera renders into.
			 * @param nearPlane Distance to the near plane. Keep it as large as the scene allows, it is what depth precision is spent on.
			 * @param farPlane Distance to the far plane.
			 */
			void CreatePerspectiveCamera(const float fovY, const float aspectRatio, const float nearPlane = 0.1f, const float farPlane = 1000.0f);

			/**
			 * @brief Sets the position of a 3D camera.
			 * @param newPos Position to set, in world units.
			 */
			void SetPosition3D(const glm::vec3 newPos);

			/**
			 * @brief Sets where a 3D camera looks. Pitch is clamped to +/- 89 degrees so the view direction can never line up with the world up axis.
			 * @param yaw Rotation around the world up axis, in degrees. 0 looks down +X.
			 * @param pitch Tilt away from the XY plane, in degrees. Positive looks up.
			 */
			void SetYawPitch(const float yaw, const float pitch);

			/**
			 * @brief Changes the vertical field of view of a 3D camera and rebuilds its projection.
			 * @param fovY Vertical field of view, in degrees.
			 */
			void SetFovY(const float fovY);

			/**
			 * @brief Changes the aspect ratio of a 3D camera and rebuilds its projection. Call this when the target the camera renders into is resized.
			 * @param aspectRatio Width over height of the target.
			 */
			void SetAspectRatio(const float aspectRatio);

			/**
			 * @brief Gets the current position of a 3D camera.
			 * @return Current position in world units.
			 */
			[[nodiscard]] glm::vec3 GetPosition3D() const;

			/**
			 * @brief Gets the current yaw of a 3D camera.
			 * @return Yaw in degrees.
			 */
			[[nodiscard]] float GetYaw() const;

			/**
			 * @brief Gets the current pitch of a 3D camera.
			 * @return Pitch in degrees.
			 */
			[[nodiscard]] float GetPitch() const;

			/**
			 * @brief Gets the direction a 3D camera looks in.
			 * @return Normalized forward vector.
			 */
			[[nodiscard]] glm::vec3 GetForward() const;

			/**
			 * @brief Gets the direction to the right of a 3D camera.
			 * @return Normalized right vector.
			 */
			[[nodiscard]] glm::vec3 GetRight() const;

			/**
			 * @brief Gets the up axis of the world. Constant, the engine treats Z as up.
			 * @return World up vector.
			 */
			[[nodiscard]] static constexpr glm::vec3 GetWorldUp() {
				return { 0.0f, 0.0f, 1.0f };
			}

			/**
			 * @brief Gets the mode the camera currently runs in.
			 * @return Current camera mode.
			 */
			[[nodiscard]] CameraMode GetMode() const;

			/**
			 * @brief Gets the view matrix, as of the last RecalculateVP call.
			 * @return Const reference to the view matrix.
			 */
			[[nodiscard]] const glm::mat4& GetViewMatrix() const;

			/**
			 * @brief Gets the projection matrix. For a 3D camera this is reverse Z and already Y flipped, so it is ready to be handed to the renderer as is.
			 * @return Const reference to the projection matrix.
			 */
			[[nodiscard]] const glm::mat4& GetProjectionMatrix() const;

			/**
			 * @brief Gets the combined view projection matrix, as of the last RecalculateVP call.
			 * @return Const reference to the view projection matrix.
			 */
			[[nodiscard]] const glm::mat4& GetViewProjectionMatrix() const;

			/**
			 * @brief Sets the position of the camera.
			 * @param newPos Position to set.
			 */
			void SetPosition(const glm::vec2 newPos);


			/**
			 * @brief Sets the rotational angle of the camera.
			 * @param angle Angle in degrees.
			 */
			void SetRotation(const float angle);

			/**
			 * @brief Changes camera scale to the requested value.
			 * @param scale Scale to apply.
			 */
			void SetScale(const glm::vec2 scale);


			/**
			 * @brief Gets the current camera position.
			 * @return Current position.
			 */
			[[nodiscard]] glm::vec2 GetPosition() const;

			/**
			 * @brief Gets the current camera rotation.
			 * @return Current rotation in degrees.
			 */
			[[nodiscard]] float GetRotation() const;

			/**
			 * @brief Gets the current camera scale.
			 * @return Current scale.
			 */
			[[nodiscard]] glm::vec2 GetScale() const;

			/**
			 * @brief Gets the current camera size.
			 * @return Current camera size, values are always positive.
			 */
			[[nodiscard]] glm::vec2 GetSize() const;

			/**
			 * @brief Recalculates the view and view projection matrices, from whichever set of state the active mode uses.
			 * @details It is necessarily to call after you've changes camera properties to apply then. The projection itself is not rebuilt here, that happens in the Create*Camera calls and in the 3D setters that invalidate it.
			 */
			void RecalculateVP();

		private:
			friend World::Scene;
			CameraController() = default;

			void RebuildPerspectiveProjection();
			void BindCameraComponent(World::Components::Scene::Camera* camera) {
				m_CurrentCameraComponent = camera;
			}

			World::Components::Scene::Camera* m_CurrentCameraComponent{ nullptr };

		};
	}
}
