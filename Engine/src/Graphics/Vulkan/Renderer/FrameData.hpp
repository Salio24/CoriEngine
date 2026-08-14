#pragma once
#include "SceneRenderer.hpp"
#include "CameraSnapshot.hpp"
#include "ThumbnailRect.hpp"
#include "PickRequest.hpp"
#include "HighlightRequest.hpp"

namespace Cori {
	namespace Graphics {
		struct Patch {
			Core::Handle<RenderObject> handle;
			std::optional<Core::AssetRef<Mesh>> mesh;
			std::optional<Core::AssetRef<Material>> material;
			glm::mat4 transform;
			glm::vec4 uvOffsets;
			uint32_t entityID{ s_NullEntityID };
			bool isNewTransform{ false };
			bool isNewUvOffsets{ false };
			bool isRegisterRequest{ false };
		};

		struct FrameData {
			FrameData() {
				patches.reserve(256);
				deletedObjects.reserve(256);
				highlights.reserve(4);
			}

			std::vector<Patch> patches;
			std::vector<Core::Handle<RenderObject>> deletedObjects;
			std::vector<HighlightRequest> highlights;
			std::optional<CameraSnapshot> cameraSnapshot;
			std::optional<vk::Extent2D> resizeRequest;
			std::optional<ThumbnailRect> thumbnailCopy;
			std::optional<PickRequest> pickRequest;

			void Clear() {
				patches.clear();
				deletedObjects.clear();
				highlights.clear();
				cameraSnapshot = {};
				resizeRequest.reset();
				thumbnailCopy.reset();
				pickRequest.reset();
			}
		};
	}
}
