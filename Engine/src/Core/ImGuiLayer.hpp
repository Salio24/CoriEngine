#pragma once
#include "Core/Layer.hpp"

namespace Cori {
	namespace Core {
		namespace Internal {
			class ImGuiLayer final : public Layer {
			public:
				ImGuiLayer();
				~ImGuiLayer() override;
				void OnAttach() override;
				void OnDetach() override;
				void OnEvent(Event& event) override;

				void StartFrame();
				void EndFrame();
			};
		}
	}
}