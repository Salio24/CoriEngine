#pragma once
#include "CameraComponent.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace World {
		class Scene;
	}

	namespace Graphics {
		/**
		 * @brief A class that is used to manipulate Camera of the Scene. Each Scene has one of those.
		 */
		class CameraController {
		public:
			~CameraController() = default;

			/**
			 * @brief Creates or recreate an orthographic camera.
			 * @param left Left camera border.
			 * @param right Right camera border.
			 * @param bottom Bottom camera border.
			 * @param top Top camera border.
			 * @param depth Depth of the camera. All object that have a depth higher that this value will not be rendered.
			 */
			void CreateOrthoCamera(const float left, float right, const float bottom, const float top, const uint8_t depth = 50);

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
			glm::vec2 GetPosition() const;

			/**
			 * @brief Gets the current camera rotation.
			 * @return Current rotation in degrees.
			 */
			float GetRotation() const;

			/**
			 * @brief Gets the current camera scale.
			 * @return Current scale.
			 */
			glm::vec2 GetScale() const;

			/**
			 * @brief Gets the current camera size.
			 * @return Current camera size, values are always positive.
			 */
			glm::vec2 GetSize() const;

			/**
			 * @brief Recalculates the view projection matrix.
			 * @details It is necessarily to call after you've changes camera properties to apply then.
			 */
			void RecalculateVP();

		private:
			friend World::Scene;
			CameraController() = default;
			void BindCameraComponent(World::Components::Scene::Camera* camera) {
				m_CurrentCameraComponent = camera;
			}

			World::Components::Scene::Camera* m_CurrentCameraComponent{ nullptr };

		};
	}
}
