#pragma once 
#include "../RenderingContext.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/AutoRegisteringFactory.hpp"

namespace Cori {
	class OpenGLContext : public RenderingContext, public Profiling::Trackable<OpenGLContext, RenderingContext>, public RegisterInUniqueFactory<RenderingContext, OpenGLContext, GraphicsAPIs, GraphicsAPIs::OpenGL> {
	public:
		static bool PreCreateHook();
		OpenGLContext();
		~OpenGLContext() override;
		void Init(SDL_Window* window) override;
		void SwapBuffers() override;
		[[nodiscard]] void* GetNativeContext() const override { return m_Context; }
	private:
		SDL_GLContext m_Context{ nullptr };

		CORI_REGISTERED_FACTORY_INIT;
	};
}