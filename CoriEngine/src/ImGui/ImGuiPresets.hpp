#pragma once
#include "Core/Application.hpp"

namespace Cori {
	namespace ImGuiPresets {
		[[maybe_unused]] static void Box2dDebugDraw(const glm::vec2 cameraBounds, const int32_t pixelsPerMeter, Core::Layer* thisPtr, const bool mouseDrag, const glm::vec2 cameraPos, const float mouseForce = 1000.0f) {
			thisPtr->m_DebugImGuiRenderer.ViewportCalc(cameraBounds, pixelsPerMeter, cameraPos);
			thisPtr->m_DebugImGuiRenderer.DrawShapes(thisPtr->ActiveScene.GetPhysicsWorld());
			if (mouseDrag) {
				if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive()) {
					thisPtr->m_DebugImGuiRenderer.HandleMouseDrag(thisPtr->ActiveScene.GetPhysicsWorld(), mouseForce);
				}
				thisPtr->m_DebugImGuiRenderer.DrawModeToggles();
			}
		}


		[[maybe_unused]] static void ScreenModeAndResolutionDropdowns() {
			const char* items[] = {"Windowed", "Borderless Windowed", "Exclusive Fullscreen"};

			int32_t oneToDisplay = 0;
			const Core::WindowMode currentMode = Core::Application::GetWindow().GetWindowMode();
			if (currentMode == Core::WindowMode::WINDOWED) {
				oneToDisplay = 0;
			}
			else if (currentMode == Core::WindowMode::BORDERLESS_WINDOWED) {
				oneToDisplay = 1;
			}
			else if (currentMode == Core::WindowMode::EXCLUSIVE_FULLSCREEN) {
				oneToDisplay = 2;
			}

			const char* windowModeDropdownPreview = items[oneToDisplay];
			if (ImGui::BeginCombo("Window Mode", windowModeDropdownPreview)) {
				for (int32_t i = 0; i < IM_ARRAYSIZE(items); i++) {
					const bool isSelected = oneToDisplay == i;
					if (ImGui::Selectable(items[i], isSelected)) {
						if (i == 0) {
							if (currentMode != Core::WindowMode::WINDOWED) {
								const auto result = Core::Application::GetWindow().SetWindowMode(Core::WindowMode::WINDOWED);
								if (!result) {
									CORI_ERROR("Failed to set window mode. Error: {}", result.error().what());
								}
							}
						}
						else if (i == 1) {
							if (currentMode != Core::WindowMode::BORDERLESS_WINDOWED) {
								const auto result = Core::Application::GetWindow().SetWindowMode(Core::WindowMode::BORDERLESS_WINDOWED);
								if (!result) {
									CORI_ERROR("Failed to set window mode. Error: {}", result.error().what());
								}
							}
						}
						else if (i == 2) {
							if (currentMode != Core::WindowMode::EXCLUSIVE_FULLSCREEN) {
								const auto result = Core::Application::GetWindow().SetWindowMode(Core::WindowMode::EXCLUSIVE_FULLSCREEN);
								if (!result) {
									CORI_ERROR("Failed to set window mode. Error: {}", result.error().what());
								}
							}
						}
					}

					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}


			static std::vector<Core::ScreenMode> screenModes;

			{
				static bool oneshot = true;
				if (oneshot) {
					screenModes = Core::Application::GetWindow().GetScreenModes();
					oneshot = false;
				}
			}

			static int32_t resolutionDropdownIdx = 0;

			ImGui::BeginDisabled(currentMode == Core::WindowMode::BORDERLESS_WINDOWED);
			const char* resolutionDropdownPreview = screenModes.at(resolutionDropdownIdx).m_ModeName.c_str();
			if (ImGui::BeginCombo("Resolution", resolutionDropdownPreview)) {
				for (int32_t i = 0; i < screenModes.size(); i++) {
					const bool isSelected = resolutionDropdownIdx == i;
					if (ImGui::Selectable(screenModes[i].m_ModeName.c_str(), isSelected)) {
						resolutionDropdownIdx = i;
						const auto result = Core::Application::GetWindow().SetScreenMode(screenModes[resolutionDropdownIdx]);
						if (!result) {
							CORI_ERROR("Failed to set screen mode. Error: {}", result.error().what());
						}
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
