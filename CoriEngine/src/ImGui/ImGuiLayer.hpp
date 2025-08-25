#pragma once
#include "Core/Layer.hpp"

namespace Cori {
	class ImGuiLayer final : public Layer {
	public:
		ImGuiLayer();
		~ImGuiLayer() override;
		void OnAttach() override;
		void OnDetach() override;
		void OnImGuiRender(const double deltaTime) override;
		void OnEvent(Event& event) override;

		void StartFrame();
		void EndFrame();
	};


}