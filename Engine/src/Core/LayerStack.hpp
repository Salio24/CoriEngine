#pragma once
#include "Layer.hpp"

namespace Cori {
	namespace Core {
		class LayerStack {
		public:
			LayerStack();
			~LayerStack();

			void PushLayer(Layer* layer);
			void PushOverlay(Layer* overlay);
			void PopLayer();
			void PopOverlay();

			std::expected<void, CoriError<>> PushLayerToQueue(Layer* layer);
			std::expected<void, CoriError<>> PushOverlayToQueue(Layer* overlay);
			void PopLayerToQueue();
			void PopOverlayToQueue();

			void ProcessQueue();

			void ClearStack();

			std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
			std::vector<Layer*>::iterator end() { return m_Layers.end(); }
			std::vector<Layer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
			std::vector<Layer*>::reverse_iterator rend() { return m_Layers.rend(); }

		private:

			std::vector<Layer*> m_Layers;

			std::vector<Layer*> m_LayerPushQueue;
			uint32_t m_LayerPopQueue{ 0 };
			std::vector<Layer*> m_OverlayPushQueue;
			uint32_t m_OverlayPopQueue{ 0 };

			uint32_t m_LayerInsertIndex{ 0 }; // also a regular layer count
			uint32_t m_OverlayLayerCount{ 0 };

			std::vector<Layer*>::iterator m_LayerInsert{ m_Layers.begin() };
		};
	}
}