#pragma once

namespace Snowflake {
	namespace ViewGizmo {
		struct Vector3 {
			float x{ 0.0f };
			float y{ 0.0f };
			float z{ 0.0f };
		};

		struct Result {
			Vector3 position;
			Vector3 forward;
			bool modified{ false };
		};

		void Initialize();

		void BeginFrame();

		[[nodiscard]] bool IsUsing();

		[[nodiscard]] bool IsOver();

		[[nodiscard]] Result Rotate(const Vector3 position, const Vector3 forward, const Vector3 up, const Vector3 pivot, const float centerX, const float centerY);
	}
}
