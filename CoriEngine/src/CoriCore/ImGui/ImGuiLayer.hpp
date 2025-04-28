#pragma once
#include "../Layer.hpp"


namespace Cori {
	class ImGuiLayer : public Layer {
	public:
		ImGuiLayer();
		~ImGuiLayer();
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate() override;
		virtual void OnImGuiRender() override;

		void StartFrame();
		void EndFrame();
	};


}