#pragma once
#include "Graphics/Texture.hpp"
#include "Utility/PathDefines.hpp"

namespace Cori {
	namespace Internal {
		namespace AssetPlaceholders {
			inline const Graphics::Texture2D::Descriptor Texture2D {
				"Placeholder Texture",
				Utility::Internal::PathDefines::PlaceholderTexture
			};
		}
	}
}