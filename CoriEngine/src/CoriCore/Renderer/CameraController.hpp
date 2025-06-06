#pragma once

#include "../ECS/Components.hpp"

namespace Cori {
	class CameraController {
	public:

		static void CreateOrthoCamera(int left, int right, int bottom, int top, float zNear = -1.0f, float zFar = 1.0f) {
			Get().CreateOrthoCameraImpl(left, right, bottom, top, zNear, zFar);
		}

		static void SetPosition(const glm::vec2 newPos) {
			Get().SetPositionImpl(newPos);
		}

		static void SetPosition(const float angle) {
			Get().SetRotationImpl(angle);
		}

		static void SetZoomLevel(const float factor) {
			Get().SetZoomLevelImpl(factor);
		}

		static void BindCameraComponent(CameraContextComponent* camera) {
			Get().BindCameraComponentImpl(camera);
		}

		static void UnbindCameraComponent() {
			Get().UnbindCameraComponentImpl();
		}

	private:
		CameraController() = default;
		~CameraController() = default;

		static CameraController& Get() {
			static CameraController instance;
			return instance;
		}

		void CreateOrthoCameraImpl(int left, int right, int bottom, int top, float zNear, float zFar);

		void SetPositionImpl(const glm::vec2 newPos);

		void SetRotationImpl(const float angle);

		void SetZoomLevelImpl(const float factor);

		void BindCameraComponentImpl(CameraContextComponent* camera);

		void UnbindCameraComponentImpl();

		void RecalculateVP();

		CameraContextComponent* m_CurrentCameraComponent{ nullptr };
		bool m_Bound{ false };

	};
}
