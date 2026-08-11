#pragma once
#include "SceneRenderer.hpp"
#include "CameraSnapshot.hpp"
#include "ThumbnailRect.hpp"

namespace Cori {
	namespace Graphics {
		struct Patch {
			Core::Handle<RenderObject> handle;
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
			std::vector<Core::Handle<RenderObject>> deletedObjects;
			std::optional<CameraSnapshot> cameraSnapshot;
			std::optional<vk::Extent2D> resizeRequest;
			std::optional<ThumbnailRect> thumbnailCopy;

			void Clear() {
				patches.clear();
				deletedObjects.clear();
				cameraSnapshot = {};
				resizeRequest.reset();
				thumbnailCopy.reset();
			}
		};
	}
}
