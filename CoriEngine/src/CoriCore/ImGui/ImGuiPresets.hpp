#pragma once

namespace Cori {
	namespace ImGuiPresets {
		inline static void Box2dDebugDraw(glm::vec2 cameraBounds, const int pixelsPerMeter, const Layer* thisPtr, const bool mouseDrag = false, const float mouseForce = 1000.0f) {
			thisPtr->debug_renderer.ViewportCalc(cameraBounds, pixelsPerMeter);
			thisPtr->debug_renderer.DrawShapes(thisPtr->ActiveScene->PhysicsWorld);
			if (mouseDrag) {
				if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive()) {
					thisPtr->debug_renderer.HandleMouseDrag(thisPtr->ActiveScene->PhysicsWorld, mouseForce);
				}
				thisPtr->debug_renderer.DrawModeToggles();
			}
		}
	}
}