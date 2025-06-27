#pragma once 
#include "GraphicsAPIs.hpp"


namespace Cori {
	class RenderingContext {
	public:
	
		virtual ~RenderingContext() {}
		virtual void Init(SDL_Window* window) = 0; 
		virtual void SwapBuffers() = 0;
		virtual inline void* GetNativeContext() const = 0;
		static std::unique_ptr<RenderingContext> Create(GraphicsAPIs api);
	};
}