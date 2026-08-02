#pragma once
#include "Core/Application.hpp"
#include "WorldSystem/Systems/Physics.hpp"

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
			auto system = layer->ActiveScene.GetSystem<World::Systems::PhysicsSystem>();
			if (system) {
				layer->m_DebugImGuiRenderer.DrawShapes(system->lock()->GetWorld());
				if (mouseDrag) {
					if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive()) {
						layer->m_DebugImGuiRenderer.HandleMouseDrag(system->lock()->GetWorld(), mouseForce);
					}
					layer->m_DebugImGuiRenderer.DrawModeToggles();
				}
			}
		}


		/**
		 * @brief Displays a window mode and screen mode selection window.
		 * @note Only usable in Layer OnImGuiRender method.
		 */
		[[maybe_unused]] static void ScreenModeAndResolutionDropdowns() {
			const char* items[] = {"Windowed", "Borderless Windowed", "Exclusive Fullscreen", "Resizable"};

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
			else if (currentMode == Core::WindowMode::RESIZABLE) {
				oneToDisplay = 3;
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
						else if (i == 3) {
							if (currentMode != Core::WindowMode::RESIZABLE) {
								const auto result = Core::Application::GetWindow().SetWindowMode(Core::WindowMode::RESIZABLE);
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

			Core::Window& window = Core::Application::GetWindow();

			ImGui::BeginDisabled(currentMode == Core::WindowMode::BORDERLESS_WINDOWED || currentMode == Core::WindowMode::RESIZABLE);
			const char* resolutionDropdownPreview = window.GetScreenModes().at(window.GetCurrentScreenMode()).m_ModeName.c_str();
			if (ImGui::BeginCombo("Resolution", resolutionDropdownPreview)) {
				for (int32_t i = 0; i < window.GetScreenModes().size(); i++) {
					const bool isSelected = window.GetCurrentScreenMode() == i;
					if (ImGui::Selectable(window.GetScreenModes()[i].m_ModeName.c_str(), isSelected)) {
						const auto result = window.SetScreenMode(i);
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

		[[maybe_unused]] static void FpsCounter(const Core::GameTimer& timer) {
			static int frameCount = 0;
			static double lastTime = 0.0;
			static float fps = 0.0f;

			frameCount++;
			double currentTime = timer.GetElapsedSeconds();
			double elapsed = currentTime - lastTime;

			if (elapsed >= 1.0) {
				fps = static_cast<float>(frameCount / elapsed);
				frameCount = 0;
				lastTime = currentTime;
			}

			ImGui::Separator();

			ImGui::Text("FPS: %.2f", fps);

			ImGui::Separator();

		}
	}
}
