#include "ImGuiLayer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include "Core/Application.hpp"

namespace Cori {
	namespace Core {
		ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO(); (void)io;

			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable multi-viewport
			// big L wayland

			ImGui::StyleColorsDark();

			ImGuiStyle& style = ImGui::GetStyle();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				style.WindowRounding = 0.0f;
				style.Colors[ImGuiCol_WindowBg].w = 1.0f;
			}

			ImGui_ImplSDL3_InitForOpenGL(static_cast<SDL_Window*>(Core::Application::GetWindow().GetNativeWindow()), Core::Application::GetWindow().GetNativeContext());

			const bool success = ImGui_ImplOpenGL3_Init("#version 460");
			CORI_CORE_ASSERT(success, "Failed to initialize ImGui with OpenGL.");

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer created");
		}

		ImGuiLayer::~ImGuiLayer() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer destroyed");
		}

		void ImGuiLayer::OnAttach() {
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer attached");
		}

		void ImGuiLayer::OnDetach() {
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::ImGui }, "ImGuiLayer detached");
		}

		void ImGuiLayer::OnImGuiRender([[maybe_unused]] const double deltaTime) {

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

		// ReSharper disable once CppMemberFunctionMayBeStatic
		void ImGuiLayer::StartFrame() {
			CORI_PROFILE_FUNCTION();
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();
		}

		// ReSharper disable once CppMemberFunctionMayBeStatic
		void ImGuiLayer::EndFrame() {
			CORI_PROFILE_FUNCTION();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			const ImGuiIO& io = ImGui::GetIO();
			(void)io;

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
				const SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
				CORI_CORE_DEBUG("ImGuiLayer rendering");
			}
		}
	}
}
