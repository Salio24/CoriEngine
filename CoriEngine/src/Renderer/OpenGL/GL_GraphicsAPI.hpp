#pragma once
#include "../CoriGraphicsAPI.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/AutoRegisteringFactory.hpp"

namespace Cori {
	class OpenGLGraphicsAPI : public CoriGraphicsAPI, public Profiling::Trackable<OpenGLGraphicsAPI, CoriGraphicsAPI>, public RegisterInUniqueFactory<CoriGraphicsAPI, OpenGLGraphicsAPI, GraphicsAPIs, GraphicsAPIs::OpenGL> {
	public:
		static bool PreCreateHook(); 
		OpenGLGraphicsAPI();

		virtual void Init() override;

		virtual void SetViewport(int x, int y, int width, int height) override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void ClearFramebuffer() override;
		
		virtual void DrawElements(const std::shared_ptr<VertexArray>& vertexArray, const uint32_t elementCount) override;


		virtual void EnableDepthTest() override;
		virtual void DisableDepthTest() override;

		virtual void EnableBlending() override;
		virtual void DisableBlending() override;

		CORI_REGISTERED_FACTORY_INIT;
	};
}