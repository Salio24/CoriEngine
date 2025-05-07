#pragma once 
#include "../RenderingContext.hpp"

namespace Cori {
	class OpenGLContext : public RenderingContext {
	public:
		OpenGLContext();
		virtual ~OpenGLContext();
		virtual void Init(SDL_Window* window) override;
		virtual void SwapBuffers() override;
		virtual inline void* GetNativeContext() const override { return static_cast<void*>(m_Context); }
	private:
		SDL_GLContext m_Context{ nullptr };
	};
}