// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "ImGuiLayer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include "../WindowImpl.hpp"
#include "../Application.hpp"
#include "../Renderer/OpenGL/GL_GraphicsContext.hpp"

namespace Cori {
	ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {
		
	}

	ImGuiLayer::~ImGuiLayer() {
		
	}

	void ImGuiLayer::OnAttach() {
		static bool created = false;
		if (CORI_CORE_ASSERT_ERROR(!created, "ImGui context already created")) { return; }

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls 
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable multi-viewport

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		
		ImGui_ImplSDL3_InitForOpenGL(static_cast<SDL_Window*>(Application::GetWindow().GetNativeWindow()), static_cast<OpenGLContext*>(Application::GetWindow().GetNativeContext()));
		// do an assert
		bool test = ImGui_ImplOpenGL3_Init("#version 460");

		created = true;
		CORI_CORE_DEBUG("ImGuiLayer attached");
	}

	void ImGuiLayer::OnDetach() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		CORI_CORE_DEBUG("ImGuiLayer detached");
	}

	void ImGuiLayer::OnImGuiRender(const double deltaTime) {
		static bool show = true;
		static bool show_ui = true;
		
		if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
			show_ui = !show_ui;
		}
		if (show_ui) {
			//ImGui::ShowDemoWindow(&show);
		}
	}

	void ImGuiLayer::OnEvent(Event& e) {
		ImGuiIO& io = ImGui::GetIO();

		if (e.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse) {
			e.m_Handeled = true;
		}
		else if (e.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard) {
			e.m_Handeled = true;
		}
	}

	void ImGuiLayer::StartFrame() {
		CORI_PROFILE_FUNCTION();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::EndFrame() {
		CORI_PROFILE_FUNCTION();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
			SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
		}
	}

}