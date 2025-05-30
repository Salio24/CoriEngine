#include <iostream>
#include <Cori.hpp>
#include <imgui.h>
#include <memory>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <imgui_internal.h>
#include <algorithm>

namespace Cori {
	namespace Shaders {

	}
	
	namespace Texture2Ds {
		inline const Texture2DDescriptor TestBrickTexture {
			"Test Brick Texture",
			"assets/engine/textures/brick.png"
		};
		inline const Texture2DDescriptor TestTileset{
			"Test Tileset 32",
			"assets/engine/textures/testTileset32.png"
		};
	}
	
	namespace Images {

	}
	
	namespace SpriteAtlases {
		inline const SpriteAtlasDescriptor test {
			"test sprite atlas",
			Texture2Ds::TestTileset,
			{32, 32}
		};
	}
}

class ExampleLayer : public Cori::Layer {
public:
	ExampleLayer() : Layer("Example") { 
		m_Camera.SetCameraSize(0, 7680, 0, 4320);

		// can also preload assets like this vvv
		Cori::AssetManager::PreloadTexture2Ds({
			Cori::Texture2Ds::TestBrickTexture
			});

		Cori::AssetManager::GetSpriteAtlas(Cori::SpriteAtlases::test);

		// if the asset is not preloaded it will be loaded the first time it is requested via appropriate
		// Get function from the Asset Manager
		Cori::GraphicsCall::SetViewport(0, 0, Cori::Application::GetWindow().GetWidth(), Cori::Application::GetWindow().GetHeight());

	}

	virtual void OnEvent(Cori::Event& event) override {
		if (!event.IsOfType(Cori::EventType::MouseMoved)) {
			CORI_TRACE("| Layer: {0} | Event: {1}", this->GetName(), event);
		}

		Cori::EventDispatcher dispatcher(event);
		dispatcher.Dispatch<Cori::MouseScrolledEvent>([this](const Cori::MouseScrolledEvent& e) -> bool {
			m_Camera.ZoomVP(std::clamp((m_Camera.GetZoomFactor() + e.GetYOffset() * 0.0625f), 0.0625f, 1000.0f));
			return true;
		});

		dispatcher.Dispatch<Cori::KeyReleasedEvent>([this](const Cori::KeyReleasedEvent& e) -> bool {
			if (e.GetKeyCode() == Cori::CORI_KEY_P) {
				CORI_PROFILE_REQUEST_NEXT_FRAME();
			}
			return false;
		});
	}

	virtual void OnImGuiRender(const double deltaTime) override {
		CORI_PROFILE_FUNCTION();
		ImGui::Begin("Performance metrics");

		ImGui::SeparatorText("Frametime graph (of last 500 frames)");

		// peak production code is happening here, lmao
		ImVec2 avail = ImGui::GetContentRegionAvail();

		float width = avail.x;
		float height = width / (14.0f / 5.0f);
		if (height > avail.y) {
			height = avail.y;
			width = height * (14.0f / 5.0f);
		}

		static const int HISTORY_SIZE = 500;
		static float frametimeHistory[HISTORY_SIZE] = {};
		static int frametimeHistoryOffset = 0;
		static int frametimeHistoryCount = 0;

		frametimeHistory[frametimeHistoryOffset] = deltaTime * 1000.0f;
		frametimeHistoryOffset = (frametimeHistoryOffset + 1) % HISTORY_SIZE;
		if (frametimeHistoryCount < HISTORY_SIZE) {
			frametimeHistoryCount++;
		}

		static float max_scale = 33.0f;
		float true_max = 0.0f;
		for (float v : frametimeHistory) true_max = ImMax(true_max, v);
		float target = ImMax(true_max * 1.2f, 1.0f);
		max_scale = ImMax(ImLerp(max_scale, target, ImClamp((float)deltaTime * 3.0f, 0.0f, 1.0f)), 4.0f);

		{
			char overlay[16];
			sprintf(overlay, "##");
			ImGui::PlotLines("##", frametimeHistory, frametimeHistoryCount, (frametimeHistoryCount == HISTORY_SIZE) ? frametimeHistoryOffset : 0, overlay, 0.0f, max_scale, ImVec2(width - 60.0f, height));
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 pos0 = ImGui::GetItemRectMin();
		ImVec2 pos1 = ImGui::GetItemRectMax();

		const int numLines = 8;
		float step = (max_scale) / numLines;

		for (int i = 0; i <= numLines; i++) {
			float t = (float)i / (float)numLines;
			float y = ImLerp(pos1.y, pos0.y, t);

			drawList->AddLine(
				ImVec2(pos0.x, y),
				ImVec2(pos1.x, y),
				IM_COL32(200, 200, 200, 100)
			);

			float value = step * i;

			char buf[16];
			sprintf(buf, "%.2f ms", value);
			ImVec2 text_size = ImGui::CalcTextSize(buf);
			drawList->AddText(
				ImVec2(pos1.x + 5, y - text_size.y * 0.5f),
				IM_COL32(180, 180, 180, 255),
				buf
			);
		}

		static double accumulator = 0.0;
		static double avgDeltaTime = 0.0;
		static double avgFPS = 0.0;
		static double timer = 0.0;

		static int frameCount = 0;

		accumulator += deltaTime;
		frameCount++;
		timer += deltaTime;

		if (timer >= 0.1f) {
			avgDeltaTime = accumulator / frameCount;
			avgFPS = frameCount / timer;

			timer = 0.0;
			accumulator = 0.0;
			frameCount = 0;
		}

		ImGui::SeparatorText("Performance averages from last 100ms");
		ImGui::Text("Avg Frametime: %.3f ms", avgDeltaTime * 1000);
		ImGui::Text("Avg FPS: %.3f", avgFPS);

		ImGui::End();

		ImGui::Begin("Renderer");
		ImGui::SeparatorText("Renderer stats");
		ImGui::Text("Total Draw Calls: %i", Cori::Renderer2D::GetDrawCallCount());
		ImGui::Text("Drawing %i textured quads", m_QuadRows * m_QuadColumns);
		ImGui::SeparatorText("Here you can manipulate how many quads are being drawn");
		ImGui::DragInt("Number of rows", &m_QuadRows, 1, 1, 2000, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragInt("Number of columns", &m_QuadColumns, 1, 1, 2000, "%d", ImGuiSliderFlags_AlwaysClamp);

		// vvv this is for debugging DONT TOUCH THAT
		//static int i0 = 2500;
		//ImGui::InputInt("input int", &i0);
		//
		//if (ImGui::Button("Button")) {
		//	Cori::Renderer2D::SetQuadsPerDraw(i0);
		//	Cori::Renderer2D::Init();
		//}

		ImGui::End();
		ImGui::Begin("Some Notes");

		ImGui::TextWrapped("You can drag ui (ImGui) floating windows outside of the main window, but be aware that doing this will slightly impact performance. \n"
							"Controls: WASD to move camera, mouse scroll-wheel to zoom in and out");

		

		ImGui::End();
	}

	void OnUpdate(const double deltaTime) override {
		CORI_PROFILE_FUNCTION();
		Cori::GraphicsCall::SetClearColor({ 0.875f, 0.6875f, 1.0f, 1.0f });
		Cori::GraphicsCall::ClearFramebuffer();

		Cori::Renderer2D::ResetDebugStats();

		Cori::Renderer2D::BeginBatch(m_Camera.GetViewProjectionMatrix());

		float offset = 5.0f;

		for (int i = 0; i < m_QuadRows; i++) {
			for (int y = 0; y < m_QuadColumns; y++) {
				//Cori::Renderer2D::DrawQuad(glm::vec2(y * 30.0f + offset , i * 30.0f + offset), glm::vec2(25.0f, 25.0f), Cori::AssetManager::GetTexture2D(Cori::Texture2Ds::TestBrickTexture));
				Cori::Renderer2D::DrawQuad(glm::vec2(y * 30.0f + offset, i * 30.0f + offset), glm::vec2(25.0f, 25.0f), Cori::AssetManager::GetSpriteAtlas(Cori::SpriteAtlases::test), 23);
				
				//Cori::Renderer2D::DrawTile(testTile);
			}
		}

		Cori::Renderer2D::EndBatch();
	}

	virtual void OnTickUpdate() override {
		CORI_PROFILE_FUNCTION();
		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_8)) {
			Cori::Profiling::InstanceMetrics<Cori::SpriteAtlas>::Report();
		}

		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_9)) {
			Cori::Profiling::InstanceMetrics<Cori::VertexArray>::Report();
		}
		
		glm::vec2 cameraPosDelta = glm::vec2(0.0f);

		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_W)) {
			cameraPosDelta.y += m_CameraMoveSpeed * m_Camera.GetZoomFactor();
		}
		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_S)) {
			cameraPosDelta.y -= m_CameraMoveSpeed * m_Camera.GetZoomFactor();
		}
		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_A)) {
			cameraPosDelta.x -= m_CameraMoveSpeed * m_Camera.GetZoomFactor();
		}
		if (Cori::Input::IsKeyPressed(Cori::CORI_KEY_D)) {
			cameraPosDelta.x += m_CameraMoveSpeed * m_Camera.GetZoomFactor();
		}

		if (cameraPosDelta != glm::vec2(0.0f)) {
			m_Camera.TranslateVP(m_Camera.GetPosition() + cameraPosDelta);
		}
	}

	int m_QuadColumns{ 1 };
	int m_QuadRows{ 1 };

	Cori::OrthoCamera m_Camera;

	std::shared_ptr<Cori::Tile> testTile = Cori::Tile::Create(glm::vec2(30.0f), glm::vec2(500.0f), Cori::SpriteAtlases::test, 23);

	float m_CameraMoveSpeed = 10.0f;

};

class Sandbox : public Cori::Application {
public:
	Sandbox() {
		PushLayer(new ExampleLayer());
		CORI_INFO("Sandbox application created");
	}

	~Sandbox() {
		CORI_INFO("Sandbox application destroyed");
	}
};

Cori::Application* Cori::CreateApplication() {
	return new Sandbox();
}