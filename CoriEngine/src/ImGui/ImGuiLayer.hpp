#pragma once
#include "Core/Layer.hpp"

namespace Cori {
	class ImGuiLayer : public Layer {
	public:
		ImGuiLayer();
		~ImGuiLayer();
		void OnAttach() override;
		void OnDetach() override;
		void OnImGuiRender(const double deltaTime) override;
		void OnEvent(Event& event) override;

		void StartFrame();
		void EndFrame();
	};


}