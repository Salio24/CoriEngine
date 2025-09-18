#pragma once 
#include "GraphicsAPIs.hpp"


namespace Cori {
	namespace Graphics {
		namespace Internal {
			class RenderingContext {
			public:
				virtual ~RenderingContext() = default;

				virtual void Init(SDL_Window* window) = 0;
				virtual void SwapBuffers() = 0;
				[[nodiscard]] virtual inline void* GetNativeContext() const = 0;
				[[nodiscard]] static std::unique_ptr<RenderingContext> Create(GraphicsAPIs api);
			};
		}
	}
}