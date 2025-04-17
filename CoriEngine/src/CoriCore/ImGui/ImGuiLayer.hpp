#pragma once

#include <CoriEngine_export.hpp>
#include "../Layer.hpp"
#include "../Application.hpp"


namespace Cori {
	class CORI_ENGINE_API ImGuiLayer : public Layer {
	public:
		ImGuiLayer();
		~ImGuiLayer();
		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate() override;
		void OnEvent(Event& event) override;
	private:
		float m_Time{ 0.0f };
	};


}