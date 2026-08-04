#include "ImGuiLayer.hpp"
#include <imgui.h>
#include "Core/Application.hpp"
#include "../Graphics/Vulkan/Renderer/ImGuiRenderer.hpp"

namespace Cori {
	namespace Core {
		namespace Internal {
			ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {
				IMGUI_CHECKVERSION();

				ImGuiIO& io = ImGui::GetIO(); (void)io;

				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
				io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking
				//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable multi-viewport
				// big L wayland
				// no multi-viewport support with threading, at least for now

				ImGui::StyleColorsDark();

				ImGuiStyle &style = ImGui::GetStyle();

				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
					style.WindowRounding = 0.0f;
					style.Colors[ImGuiCol_WindowBg].w = 1.0f;
				}

				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer created");
			}

			ImGuiLayer::~ImGuiLayer() {
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer destroyed");
			}

			void ImGuiLayer::OnAttach() {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer attached");
			}

			void ImGuiLayer::OnDetach() {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer detached");
			}

			void ImGuiLayer::OnEvent(Event& event) {
				const ImGuiIO& io = ImGui::GetIO();

				if (event.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse) {
					event.m_Handled = true;
				}
				else if (event.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard) {
					event.m_Handled = true;
				}
			}

			void ImGuiLayer::StartFrame() {
				CORI_PROFILE_FUNCTION();
				Graphics::ImGuiRenderer::StartFrame();
				ImGui::NewFrame();
			}

			void ImGuiLayer::EndFrame() {
				CORI_PROFILE_FUNCTION();
				ImGui::EndFrame();
				ImGui::Render();
				Graphics::ImGuiRenderer::EndFrame();
			}
		}
	}
}
