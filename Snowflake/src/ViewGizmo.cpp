#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

#include <imgui.h>

#define IMVIEWGUIZMO_IMPLEMENTATION
#undef GLM_FORCE_INTRINSICS
#include <ImViewGuizmo.h>

#include "ViewGizmo.hpp"

namespace {
	glm::vec3 ToGlm(const Snowflake::ViewGizmo::Vector3 vector) {
		return { vector.x, vector.y, vector.z };
	}

	Snowflake::ViewGizmo::Vector3 FromGlm(const glm::vec3 vector) {
		return { vector.x, vector.y, vector.z };
	}
}

namespace Snowflake {
	namespace ViewGizmo {
		void Initialize() {
			ImViewGuizmo::Style& style = ImViewGuizmo::GetStyle();
			const ImViewGuizmo::Style defaults{};
			style.axisLabels[0] = "X";
			style.axisLabels[1] = "-X";
			style.axisLabels[2] = "Z";
			style.axisLabels[3] = "-Z";
			style.axisLabels[4] = "-Y";
			style.axisLabels[5] = "Y";

			style.axisColors[0] = defaults.axisColors[0];
			style.axisColors[1] = defaults.axisColors[2];
			style.axisColors[2] = defaults.axisColors[1];

			style.animateSnap = false;
		}

		void BeginFrame() {
			ImViewGuizmo::BeginFrame();
		}

		bool IsUsing() {
			return ImViewGuizmo::IsUsing();
		}

		bool IsOver() {
			return ImViewGuizmo::IsOver();
		}

		Result Rotate(const Vector3 position, const Vector3 forward, const Vector3 up, const Vector3 pivot, const float centerX, const float centerY) {
			glm::vec3 cameraPosition = ToGlm(position);

			glm::quat cameraRotation = glm::quatLookAt(glm::normalize(ToGlm(forward)), glm::normalize(ToGlm(up)));

			const bool modified = ImViewGuizmo::Rotate(cameraPosition, cameraRotation, ToGlm(pivot), ImVec2(centerX, centerY));

			return Result{
				.position = FromGlm(cameraPosition),
				.forward = FromGlm(glm::normalize(cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f))),
				.modified = modified
			};
		}
	}
}
