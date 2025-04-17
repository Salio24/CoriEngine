#include "ImGuiLayer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include "../WindowImpl.hpp"
#include "../Application.hpp"

namespace Cori {
	ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {
		
	}

	ImGuiLayer::~ImGuiLayer() {
		
	}

	void ImGuiLayer::OnAttach() {
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls 
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable multi-viewport

		ImGui::StyleColorsDark();
		ImGui_ImplSDL3_InitForOpenGL((SDL_Window*)Application::Get().GetWindow().GetNativeWindow(), (SDL_GLContext)Application::Get().GetWindow().GetNativeContex());
		
		// do an assert
		bool test = ImGui_ImplOpenGL3_Init("#version 460");

		CORI_CORE_INFO("ImGuiLayer attached");
	}

	void ImGuiLayer::OnDetach() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		CORI_CORE_INFO("ImGuiLayer detached");
	}

	void ImGuiLayer::OnUpdate() {

	}

	void ImGuiLayer::OnImGuiRender() {

		static bool show = true;
		ImGui::ShowDemoWindow(&show);
	}

	void ImGuiLayer::StartFrame() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::EndFrame() {
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

}