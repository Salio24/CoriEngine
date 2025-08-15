#pragma once
#include "Core/Application.hpp"

namespace Cori {
	namespace ImGuiPresets {
		[[maybe_unused]] static void Box2dDebugDraw(glm::vec2 cameraBounds, const int pixelsPerMeter, Layer* thisPtr, const bool mouseDrag, const glm::vec2 cameraPos, const float mouseForce = 1000.0f) {
			thisPtr->debug_renderer.ViewportCalc(cameraBounds, pixelsPerMeter, cameraPos);
			thisPtr->debug_renderer.DrawShapes(thisPtr->ActiveScene.GetPhysicsWorld());
			if (mouseDrag) {
				if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive()) {
					thisPtr->debug_renderer.HandleMouseDrag(thisPtr->ActiveScene.GetPhysicsWorld(), mouseForce);
				}
				thisPtr->debug_renderer.DrawModeToggles();
			}
		}


		[[maybe_unused]] static void ScreenModeAndResolutionDropdowns() {
			const char* items[] = {"Windowed", "Borderless Windowed", "Exclusive Fullscreen"};

			int oneToDisplay = 0;
			WindowMode currentMode = Application::GetWindow().GetWindowMode();
			if (currentMode == WindowMode::WINDOWED) {
				oneToDisplay = 0;
			}
			else if (currentMode == WindowMode::BORDERLESS_WINDOWED) {
				oneToDisplay = 1;
			}
			else if (currentMode == WindowMode::EXCLUSIVE_FULLSCREEN) {
				oneToDisplay = 2;
			}

			const char* windowModeDropdownPreview = items[oneToDisplay];
			if (ImGui::BeginCombo("Window Mode", windowModeDropdownPreview)) {
				for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
					const bool isSelected = oneToDisplay == i;
					if (ImGui::Selectable(items[i], isSelected)) {
						if (i == 0) {
							if (currentMode != WindowMode::WINDOWED) {
								Application::GetWindow().SetWindowMode(WindowMode::WINDOWED);
							}
						}
						else if (i == 1) {
							if (currentMode != WindowMode::BORDERLESS_WINDOWED) {
								Application::GetWindow().SetWindowMode(WindowMode::BORDERLESS_WINDOWED);
							}
						}
						else if (i == 2) {
							if (currentMode != WindowMode::EXCLUSIVE_FULLSCREEN) {
								Application::GetWindow().SetWindowMode(WindowMode::EXCLUSIVE_FULLSCREEN);
							}
						}
					}

					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}


			static std::vector<ScreenMode> screenModes;

			{
				static bool oneshot = true;
				if (oneshot) {
					screenModes = Application::GetWindow().GetScreenModes();
					oneshot = false;
				}
			}

			static int resolutionDropdownIdx = 0;

			ImGui::BeginDisabled(currentMode == WindowMode::BORDERLESS_WINDOWED);
			const char* resolutionDropdownPreview = screenModes.at(resolutionDropdownIdx).m_ModeName.c_str();
			if (ImGui::BeginCombo("Resolution", resolutionDropdownPreview)) {
				for (int i = 0; i < screenModes.size(); i++) {
					const bool isSelected = resolutionDropdownIdx == i;
					if (ImGui::Selectable(screenModes[i].m_ModeName.c_str(), isSelected)) {
						resolutionDropdownIdx = i;
						Application::GetWindow().SetScreenMode(screenModes[resolutionDropdownIdx]);
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::EndDisabled();
		}
	}
}
