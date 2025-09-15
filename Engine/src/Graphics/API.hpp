#pragma once
#include "CoriGraphicsAPI.hpp"

namespace Cori {
	namespace Core {
		class Application;
	}

	namespace Graphics {
		namespace Internal {
			class API {
			public:

				static void SetViewport(const int32_t x, const int32_t y, const int32_t width, const int32_t height) {
					s_GraphicsAPI->SetViewport(x, y, width, height);
				}

				static void SetClearColor(const glm::vec4& color) {
					s_GraphicsAPI->SetClearColor(color);
				}

				static void ClearFramebuffer() {
					s_GraphicsAPI->ClearFramebuffer();
				}

				static void DrawElementsTriangles(const uint32_t elementCount) {
					s_GraphicsAPI->DrawElementsTriangles(elementCount);
				}

				static void DrawElementsInstancedTriangles(const uint32_t instanceCount) {
					s_GraphicsAPI->DrawElementsInstancedTriangles(instanceCount);
				}

				static void EnableDepthTest() {
					s_GraphicsAPI->EnableDepthTest();
				}

				static void DisableDepthTest() {
					s_GraphicsAPI->DisableDepthTest();
				}

				static void EnableBlending() {
					s_GraphicsAPI->EnableBlending();
				}

				static void DisableBlending() {
					s_GraphicsAPI->DisableBlending();
				}

				static void SetDepthMask(const bool mode) {
					s_GraphicsAPI->SetDepthMask(mode);
				}

			protected:
				friend Core::Application;

				static void Init();
				static void Shutdown();

				static std::unique_ptr<CoriGraphicsAPI> s_GraphicsAPI;
			};
		}
	}
}
