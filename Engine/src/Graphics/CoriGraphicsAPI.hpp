#pragma once
#include <glm/glm.hpp>

namespace Cori {
	namespace Graphics {
		class CoriGraphicsAPI {
		public:
			virtual ~CoriGraphicsAPI() = default;

			virtual void Init() = 0;

			virtual void SetViewport(const int32_t x, const int32_t y, const int32_t width, const int32_t height) = 0;

			virtual void SetClearColor(const glm::vec4& color) = 0;
			virtual void ClearFramebuffer() = 0;

			virtual void DrawElementsTriangles(const uint32_t elementCount) = 0;

			virtual void DrawElementsInstancedTriangles(const uint32_t instanceCount) = 0;

			virtual void EnableDepthTest() = 0;
			virtual void DisableDepthTest() = 0;

			virtual void EnableBlending() = 0;
			virtual void DisableBlending() = 0;

			virtual void SetDepthMask(const bool mode) = 0;

			[[nodiscard]] static std::unique_ptr<CoriGraphicsAPI> Create();
		};
	}
}