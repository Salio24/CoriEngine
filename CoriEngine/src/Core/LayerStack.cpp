#include "LayerStack.hpp"

namespace Cori {
	
	LayerStack::LayerStack() {
		CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "LayerStack created.");
	}

	LayerStack::~LayerStack() {
		while (!m_Layers.empty()) {
			m_Layers.back()->OnDetach();
			delete m_Layers.back();
			m_Layers.pop_back();
		}
		CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "LayerStack destroyed.");
	}

	std::expected<void, CoriError<>> LayerStack::PushLayerToQueue(Layer* layer) {
		if (layer) {
			bool nameCollision = false;
			for (const Layer* l : m_Layers) {
				if (!nameCollision) {
					nameCollision = l->GetName() == layer->GetName();
				} else {
					break;
				}
			}
			if (nameCollision) {
				return std::unexpected{CoriError(std::format("Layer with name '{}' already exist in the LayerStack. Layers can't have duplicate names.", layer->GetName()))};
			}

			m_LayerPushQueue.push_back(layer);
			return {};
		}

		return std::unexpected(CoriError("Trying to push a null Layer."));
	}

	std::expected<void, CoriError<>> LayerStack::PushOverlayToQueue(Layer* overlay) {
		if (overlay) {
			bool nameCollision = false;
			for (const Layer* l : m_Layers) {
				if (!nameCollision) {
					nameCollision = l->GetName() == overlay->GetName();
				} else {
					break;
				}
			}
			if (nameCollision) {
				return std::unexpected{CoriError(std::format("Layer with name '{}' already exist in the LayerStack. Layers can't have duplicate names.", overlay->GetName()))};
			}

			m_OverlayPushQueue.push_back(overlay);
			return {};
		}

		return std::unexpected(CoriError("Trying to push a null Overlay Layer."));
	}

	void LayerStack::PopLayerToQueue(Layer* layer) {
		if (layer) {
			const auto it = std::ranges::find(m_Layers, layer);
			if (it != m_Layers.end()) {
				m_LayerPopQueue.push_back(layer);
				return;
			}

			CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Layer '{}' is not in the LayerStack, nothing to pop.", layer->GetName());
		}

		CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Trying to pop a null Layer.");
	}

	void LayerStack::PopOverlayToQueue(Layer* overlay) {
		if (overlay) {
			const auto it = std::ranges::find(m_Layers, overlay);
			if (it != m_Layers.end()) {
				m_LayerPopQueue.push_back(overlay);
				return;
			}
			CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Overlay Layer '{}' is not in the LayerStack, nothing to pop.", overlay->GetName());
		}

		CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Trying to pop a null Overlay Layer.");
	}

	void LayerStack::ProcessQueue() {
		if (m_LayerPushQueue.size() != 0) {
			for (Layer* layer : std::views::reverse(m_LayerPushQueue)) {
				PushLayer(layer);
			}
			m_LayerPushQueue.clear();
		}
		
		if (m_OverlayPushQueue.size() != 0) {
			for (Layer* layer : std::views::reverse(m_OverlayPushQueue)) {
				PushOverlay(layer);
			}
			m_OverlayPushQueue.clear();
		}

		if (m_LayerPopQueue.size() != 0) {
			for (Layer* layer : std::views::reverse(m_LayerPopQueue)) {
				PopLayer(layer);
			}
			m_LayerPopQueue.clear();
		}

		if (m_OverlayPopQueue.size() != 0) {
			for (Layer* layer : std::views::reverse(m_OverlayPopQueue)) {
				PopOverlay(layer);
			}
			m_OverlayPopQueue.clear();
		}
	}

	void LayerStack::ClearStack() {
		CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Clearing LayerStack.");
		while (!m_Layers.empty()) {
			m_Layers.back()->OnDetach();
			delete m_Layers.back();
			m_Layers.pop_back();
		}
		CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "LayerStack cleared.");
	}

	void LayerStack::PushLayer(Layer* layer) {
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		layer->OnAttach();
		m_LayerInsertIndex++;
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Pushed Layer '{}' to LayerStack.", layer->GetName());
	}

	void LayerStack::PushOverlay(Layer* overlay) {
		m_Layers.emplace_back(overlay);
		overlay->OnAttach();
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Pushed Overlay Layer '{}' to LayerStack.", overlay->GetName());
	}

	void LayerStack::PopLayer(Layer* layer) {
		layer->OnDetach();
		m_Layers.erase(std::ranges::find(m_Layers, layer));
		m_LayerInsertIndex--;
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Popped Layer '{}' from LayerStack.", layer->GetName());
	}

	void LayerStack::PopOverlay(Layer* overlay) {
		overlay->OnDetach();
		m_Layers.erase(std::ranges::find(m_Layers, overlay));
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Popped Overlay Layer '{}' from LayerStack.", overlay->GetName());
	}
}