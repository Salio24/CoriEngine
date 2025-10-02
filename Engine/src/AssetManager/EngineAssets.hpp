#pragma once
#include "Graphics/Texture.hpp"
#include "FileSystem/PathManager.hpp"

namespace Cori {
	namespace Internal {
		namespace AssetPlaceholders {
			inline const Graphics::Texture2D::Descriptor Texture2DPlaceholder {
				"Placeholder Texture",
				FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "placeholders/missing_texture32.png"
			};
		}
	}
}