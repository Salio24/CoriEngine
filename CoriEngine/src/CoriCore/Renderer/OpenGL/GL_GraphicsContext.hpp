#pragma once 
#include "../RenderingContext.hpp"
#include "../../Profiling/Trackable.hpp"
#include "../../AutoRegisteringFactory.hpp"

namespace Cori {
	class OpenGLContext : public RenderingContext, public Profiling::Trackable<OpenGLContext, RenderingContext>, public RegisterInUniqueFactory<RenderingContext, OpenGLContext, GraphicsAPIs, GraphicsAPIs::OpenGL> {
	public:
		virtual ~OpenGLContext();
		virtual void Init(SDL_Window* window) override;
		virtual void SwapBuffers() override;
		virtual inline void* GetNativeContext() const override { return static_cast<void*>(m_Context); }
	private:
		SDL_GLContext m_Context{ nullptr };

		CORI_DECLARE_UNIQUE_FACTORY_REGISTERED(OpenGLContext, ());
	};
}