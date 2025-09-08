#pragma once
#include "Graphics/Texture.hpp"
#include "FileSystem/PathDefines.hpp"

namespace Cori {
	namespace Internal {
		namespace AssetPlaceholders {
			inline const Graphics::Texture2D::Descriptor Texture2D {
				"Placeholder Texture",
				FileSystem::Internal::PathDefines::PlaceholderTexture
			};
		}
	}
}