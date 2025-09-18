#pragma once
#include "Graphics/Texture.hpp"
#include <PathDefinesGenerated.hpp>

namespace Cori {
	namespace Internal {
		namespace AssetPlaceholders {
			inline const Graphics::Texture2D::Descriptor Texture2DPlaceholder {
				"Placeholder Texture",
				FileSystem::Internal::PathDefines::GetEngineDataRoot() / "placeholders/missing_texture32.png"
			};
		}
	}
}