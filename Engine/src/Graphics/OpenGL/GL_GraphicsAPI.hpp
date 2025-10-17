#pragma once
#include "../CoriGraphicsAPI.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class OpenGLGraphicsAPI final : public CoriGraphicsAPI {
			public:
				OpenGLGraphicsAPI();

				void Init() override;

				void SetViewport(const int32_t x, const int32_t y, const int32_t width, const int32_t height) override;

				void SetClearColor(const glm::vec4& color) override;
				void ClearFramebuffer() override;

				void DrawElementsTriangles(const uint32_t elementCount) override;

				void DrawElementsInstancedTriangles(const uint32_t instanceCount) override;

				void EnableDepthTest() override;
				void DisableDepthTest() override;

				void EnableBlending() override;
				void DisableBlending() override;

				void SetDepthMask(const bool mode) override;
			};
		}
	}
}