#include "ImGuiLayer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include "../Platform/Windows/WindowsWindow.hpp"

namespace Cori {
	ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {
		
	}

	ImGuiLayer::~ImGuiLayer() {
		
	}

	void ImGuiLayer::OnAttach() {
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls 

		// CROSSPLATFORM: here we need to check for window type, and depending on window type, initialize imgui differently, but thats only if we want multiplatfrom, only windows for now
		WindowsWindow* winWindow = dynamic_cast<WindowsWindow*>(&Application::Get().GetWindow());
		if (!winWindow) {
			CORI_CORE_FATAL("ImGuiLayer: Window is not a WindowsWindow");
		}

		ImGui_ImplSDL3_InitForOpenGL(winWindow->GetSDLWindow(), winWindow->GetSDLGLContext());
		
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

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		
		io.DisplaySize = ImVec2((float)Application::Get().GetWindow().GetWidth(), (float)Application::Get().GetWindow().GetHeight());
		
		float time = (float)SDL_GetTicks() / 1000.0f;
		io.DeltaTime = m_Time > 0.0f ? (time - m_Time) : (1.0f / 60.0f);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		static bool show = true;
		ImGui::ShowDemoWindow(&show);

		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			//ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (show)
		{
			ImGui::Begin("Another Window", &show);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				show = false;
			ImGui::End();
		}


		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		//CORI_CORE_INFO("ImGuiLayer updated");
	}

	void ImGuiLayer::OnEvent(Event& event) {
		//CORI_CORE_TRACE("ImGuiLayer event: {0}", event);
	}
}