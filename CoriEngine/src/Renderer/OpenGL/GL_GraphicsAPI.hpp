#pragma once
#include "../CoriGraphicsAPI.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/AutoRegisteringFactory.hpp"

namespace Cori {
	class OpenGLGraphicsAPI : public CoriGraphicsAPI, public RegisterInUniqueFactory<CoriGraphicsAPI, OpenGLGraphicsAPI, GraphicsAPIs, GraphicsAPIs::OpenGL> {
	public:
		static bool PreCreateHook(); 
		OpenGLGraphicsAPI();

		void Init() override;

		void SetViewport(int x, int y, int width, int height) override;

		void SetClearColor(const glm::vec4& color) override;
		void ClearFramebuffer() override;
		
		void DrawElementsTriangles(const uint32_t elementCount) override;

		void DrawElementsInstancedTriangles(const uint32_t instanceCount) override;

		void EnableDepthTest() override;
		void DisableDepthTest() override;

		void EnableBlending() override;
		void DisableBlending() override;

		void SetDepthMask(bool mode) override;

		CORI_REGISTERED_FACTORY_INIT;
	};
}