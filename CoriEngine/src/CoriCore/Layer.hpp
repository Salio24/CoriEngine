#pragma once
#include "EventSystem/Event.hpp"

namespace Cori {

	class Layer {
	public:
		Layer(const std::string& name = "Layer");

		virtual ~Layer();

		virtual void OnAttach();
		virtual void OnDetach();
		virtual void OnUpdate(const double deltaTime) {}
		virtual void OnTickUpdate() {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& event) {}

		inline const std::string& GetName() const { return m_DebugName; }
	protected:
		std::string m_DebugName;
	};
}