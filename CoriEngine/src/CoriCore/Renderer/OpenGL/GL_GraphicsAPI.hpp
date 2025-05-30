#pragma once
#include "../CoriGraphicsAPI.hpp"
#include "../../Profiling/Trackable.hpp"
#include "../../AutoRegisteringFactory.hpp"

namespace Cori {
	class OpenGLGraphicsAPI : public CoriGraphicsAPI, public Profiling::Trackable<OpenGLGraphicsAPI, CoriGraphicsAPI>, public RegisterInUniqueFactory<CoriGraphicsAPI, OpenGLGraphicsAPI, GraphicsAPIs, GraphicsAPIs::OpenGL> {
	public:

		virtual void Init() override;

		virtual void SetViewport(int x, int y, int width, int height) override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void ClearFramebuffer() override;
		
		virtual void DrawElements(const std::shared_ptr<VertexArray>& vertexArray, const uint32_t elementCount) override;
		CORI_DECLARE_UNIQUE_FACTORY_REGISTERED(OpenGLGraphicsAPI, ());
	};
}