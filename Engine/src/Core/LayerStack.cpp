#include "LayerStack.hpp"

namespace Cori {
	namespace Core {
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
				//layer->OnAttach();
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
				//overlay->OnAttach();
				return {};
			}

			return std::unexpected(CoriError("Trying to push a null Overlay Layer."));
		}

		void LayerStack::PopLayerToQueue() {
			m_LayerPopQueue++;
		}

		void LayerStack::PopOverlayToQueue() {
			m_LayerPopQueue++;
		}

		void LayerStack::ProcessQueue() {
			if (m_LayerPushQueue.size() != 0) {
				std::vector<Layer*> layersToPush = std::move(m_LayerPushQueue);
				for (Layer* layer : std::views::reverse(layersToPush)) {
					PushLayer(layer);
					//m_LayerPushQueue.erase(std::ranges::find(m_LayerPushQueue, layer));
				}
			}

			if (m_OverlayPushQueue.size() != 0) {
				std::vector<Layer*> layersToPush = std::move(m_OverlayPushQueue);
				for (Layer* layer : std::views::reverse(layersToPush)) {
					PushOverlay(layer);
					//m_OverlayPushQueue.erase(std::ranges::find(m_OverlayPushQueue, layer));
				}
			}

			if (m_LayerPopQueue != 0) {
				for (uint32_t i = 0; i < m_LayerPopQueue; ++i) {
					PopLayer();
				}
				m_LayerPopQueue = 0;
			}

			if (m_OverlayPopQueue != 0) {
				for (uint32_t i = 0; i < m_OverlayPopQueue; ++i) {
					PopOverlay();
				}
				m_OverlayPopQueue = 0;
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
			++m_LayerInsertIndex;
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Pushed Layer '{}' to LayerStack.", layer->GetName());
		}

		void LayerStack::PushOverlay(Layer* overlay) {
			m_Layers.emplace_back(overlay);
			overlay->OnAttach();
			++m_OverlayLayerCount;
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Pushed Overlay Layer '{}' to LayerStack.", overlay->GetName());
		}

		void LayerStack::PopLayer() {
			if (m_LayerInsertIndex != 0) {
				Layer* layer = m_Layers.at(m_LayerInsertIndex - 1);
				layer->OnDetach();
				m_Layers.erase(m_Layers.begin() + m_LayerInsertIndex);
				--m_LayerInsertIndex;
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Popped Layer '{}' from LayerStack.", layer->GetName());
			}
		}

		void LayerStack::PopOverlay() {
			if (m_OverlayLayerCount != 0) {
				Layer* overlay = m_Layers.at(m_Layers.size() - 1 - m_OverlayLayerCount);
				overlay->OnDetach();
				m_Layers.erase(m_Layers.begin() + m_Layers.size() - m_OverlayLayerCount);
				--m_OverlayLayerCount;
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::LayerStack }, "Popped Overlay Layer '{}' from LayerStack.", overlay->GetName());
			}
		}
	}
}