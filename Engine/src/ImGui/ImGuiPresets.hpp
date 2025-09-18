#pragma once
#include "Core/Application.hpp"

namespace Cori {
	/**
	 * @brief Here are stored all predefined ImGui window presets, you can only use this functions in Layer OnImGuiRender method, using it anywhere else will result in a crash.
	 */
	namespace ImGuiPresets {
		/**
		 * @brief Enables the debug draw of Box2D physics.
		 * @param cameraSize Size of the debug cameras viewport, use GetSize() with your main Graphics::CameraController to align the main camera and debug camera.
		 * @param cameraPos Position of the debug camera, use GetSize() with your main Graphics::CameraController to align the main camera and debug camera.
		 * @param pixelsPerMeter Pixels in main camera per meter range. Use CORI_PIXELS_PER_METER.
		 * @param layer Pointer to the layer you're calling this function from.
		 * @param mouseDrag Enable or disable dynamic object dragging with a mouse.
		 * @param mouseForce Force to apply when using mouse drag.
		 * @note Only usable in Layer OnImGuiRender method.
		 */
		[[maybe_unused]] static void Box2dDebugDraw(const glm::vec2 cameraSize, const glm::vec2 cameraPos, const int32_t pixelsPerMeter, Core::Layer* layer, const bool mouseDrag, const float mouseForce = 1000.0f) {
			layer->m_DebugImGuiRenderer.ViewportCalc(cameraSize, pixelsPerMeter, cameraPos);
			layer->m_DebugImGuiRenderer.DrawShapes(layer->ActiveScene.GetPhysicsWorld());
			if (mouseDrag) {
				if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive()) {
					layer->m_DebugImGuiRenderer.HandleMouseDrag(layer->ActiveScene.GetPhysicsWorld(), mouseForce);
				}
				layer->m_DebugImGuiRenderer.DrawModeToggles();
			}
		}


		/**
		 * @brief Displays a window mode and screen mode selection window.
		 * @note Only usable in Layer OnImGuiRender method.
		 */
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
