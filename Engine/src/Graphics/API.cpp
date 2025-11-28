#include "API.hpp"
#include "Graphics/Renderer2D.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			std::unique_ptr<CoriGraphicsAPI> API::s_GraphicsAPI = nullptr;
			void API::Init() {
				s_GraphicsAPI = CoriGraphicsAPI::Create();
				s_GraphicsAPI->Init();
			}

			void API::Shutdown() {
				s_GraphicsAPI.reset();
			}
		}
	}
}