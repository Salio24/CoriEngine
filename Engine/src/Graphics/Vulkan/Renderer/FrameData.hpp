#pragma once
#include "SceneRenderer.hpp"
#include "CameraSnapshot.hpp"

namespace Cori {
	namespace Graphics {
		struct Patch {
			Core::Handle<SceneRenderer::RenderObject> handle;
			std::optional<Core::AssetRef<Mesh>> mesh;
			std::optional<Core::AssetRef<Material>> material;
			glm::mat4 transform;
			glm::vec4 uvOffsets;
			bool isNewTransform{ false };
			bool isNewUvOffsets{ false };
			bool isRegisterRequest{ false };
		};

		struct FrameData {
			FrameData() {
				patches.reserve(256);
				deletedObjects.reserve(256);
			}

			std::vector<Patch> patches;
			std::vector<Core::Handle<SceneRenderer::RenderObject>> deletedObjects;
			std::optional<CameraSnapshot> cameraSnapshot;
			std::optional<vk::Extent2D> resizeRequest;
			uint64_t rtcqWatermark{ 0 };

			void Clear() {
				patches.clear();
				deletedObjects.clear();
				cameraSnapshot = {};
				resizeRequest.reset();
				rtcqWatermark = 0;
			}
		};
	}
}
