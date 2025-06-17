#pragma once
#include "../Layer.hpp"
#include "ImGuiPresets.hpp"

namespace Cori {
	class ImGuiLayer : public Layer {
	public:
		ImGuiLayer();
		~ImGuiLayer();
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(const double deltaTime) override;
		virtual void OnImGuiRender(const double deltaTime) override;
		virtual void OnEvent(Event& event) override;

		void StartFrame();
		void EndFrame();
	};


}