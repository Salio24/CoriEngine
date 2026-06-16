#pragma once
#include "Renderer.hpp"
#include "../RenderThreadCommandQueue.hpp"
#include "../RenderThreadWakeup.hpp"

namespace Cori {
	namespace Graphics {
		class MasterRenderer {
		public:
			static void Init();

			static void Shutdown();

			static MasterRenderer& Get();

			~MasterRenderer() {

			}

		private:
			MasterRenderer() {

			}

			Core::FlatSlotMap<Renderer> m_Renderers;

			static std::unique_ptr<MasterRenderer> s_Instance;
		};
	}
}
