#pragma once

namespace Cori {
	namespace Graphics {
		struct CameraSnapshot {
			glm::mat4 view{ 1.0f };
			glm::mat4 projection{ 1.0f };
			vk::Extent2D viewportSize{};
		};
	}
}