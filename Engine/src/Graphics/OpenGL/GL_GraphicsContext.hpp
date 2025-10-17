#pragma once 
#include "../RenderingContext.hpp"
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class OpenGLContext final : public RenderingContext, public Profiling::Trackable<OpenGLContext, RenderingContext> {
			public:
				OpenGLContext();
				~OpenGLContext() override;
				void Init(SDL_Window* window) override;
				void SwapBuffers() override;
				[[nodiscard]] void* GetNativeContext() const override { return m_Context; }
			private:
				SDL_GLContext m_Context{ nullptr };
			};
		}
	}
}