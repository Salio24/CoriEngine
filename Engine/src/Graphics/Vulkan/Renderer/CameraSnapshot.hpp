#pragma once

namespace Cori {
	namespace Graphics {
		struct CameraSnapshot {
			glm::mat4 view{};
			glm::mat4 projection{};
			vk::Extent2D viewportSize{};
		};
	}
}