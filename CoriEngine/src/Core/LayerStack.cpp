// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "LayerStack.hpp"

namespace Cori {
	
	LayerStack::LayerStack() {
		CORI_CORE_INFO("LayerStack created");
	}

	LayerStack::~LayerStack() {
		for (Layer* layer : m_Layers) {
			layer->OnDetach();
			delete layer;
		}
		CORI_CORE_INFO("LayerStack destroyed");
	}

	void LayerStack::PushLayerToQueue(Layer* layer) {
		m_LayerPushQueue.push_back(layer);
	}

	void LayerStack::PushOverlayToQueue(Layer* overlay) {
		m_OverlayPushQueue.push_back(overlay);
	}

	void LayerStack::PopLayerToQueue(Layer* layer) {
		m_LayerPopQueue.push_back(layer);
	}

	void LayerStack::PopOverlayToQueue(Layer* overlay) {
		m_OverlayPopQueue.push_back(overlay);
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

	void LayerStack::PushLayer(Layer* layer) {
#ifdef DEBUG_BUILD
		bool duplicateName = false;
		for (Layer* l : m_Layers) {
			if (!duplicateName) {
				duplicateName = (l->GetName() == layer->GetName());
			}
		}
		if (CORI_CORE_ASSERT_ERROR(!duplicateName, "Trying to push a layer, but the layer with the specified debug name already in the LayerStack. Name: '{}'.", layer->GetName())) { delete layer; return; }
#endif

		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		layer->OnAttach();
		m_LayerInsertIndex++; 
	}

	void LayerStack::PushOverlay(Layer* overlay) {
		m_Layers.emplace_back(overlay);
		overlay->OnAttach();
	}

	void LayerStack::PopLayer(Layer* layer) {
		auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
		if (it != m_Layers.end()) {
			layer->OnDetach();
			m_Layers.erase(it);
			m_LayerInsertIndex--;
		}
	}

	void LayerStack::PopOverlay(Layer* overlay) {
		auto it = std::find(m_Layers.begin(), m_Layers.end(), overlay);
		if (it != m_Layers.end()) {
			overlay->OnDetach();
			m_Layers.erase(it);
		}
	}
}