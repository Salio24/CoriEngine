#pragma once
#include "Layer.hpp"

namespace Cori {

	class LayerStack {
	public:
		LayerStack();
		~LayerStack();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);
		
		void PushLayerToQueue(Layer* layer);
		void PushOverlayToQueue(Layer* overlay);
		void PopLayerToQueue(Layer* layer);
		void PopOverlayToQueue(Layer* overlay);

		void ProcessQueue();

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }
		std::vector<Layer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
		std::vector<Layer*>::reverse_iterator rend() { return m_Layers.rend(); }
		
	private:

		std::vector<Layer*> m_Layers;

		std::vector<Layer*> m_LayerPushQueue;
		std::vector<Layer*> m_LayerPopQueue;
		std::vector<Layer*> m_OverlayPushQueue;
		std::vector<Layer*> m_OverlayPopQueue;


		unsigned int m_LayerInsertIndex{ 0 };

		std::vector<Layer*>::iterator m_LayerInsert{ m_Layers.begin() };
	};
}