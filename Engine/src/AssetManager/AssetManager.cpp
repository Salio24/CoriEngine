#include "AssetManager.hpp"
#include "Graphics/Font.hpp"
#include "Graphics/Texture.hpp"
#include "FileSystem/PathManager.hpp"

namespace Cori {
	AssetManager::Cache* AssetManager::s_Cache = nullptr;

	void AssetManager::Init() {
		s_Cache = new Cache();
		RegisterPlaceholders();
	}

	void AssetManager::Shutdown() {
		delete s_Cache;
	}

	void AssetManager::RegisterPlaceholders() {
		const auto fp = Graphics::Font::Create(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "placeholders/unifont-16.0.04.otf", { Graphics::Font::CharsetRanges::Latin, Graphics::Font::CharsetRanges::LatinExtendedA, Graphics::Font::CharsetRanges::LatinExtendedB });
		RegisterPlaceholder<Graphics::Font>(fp);

		const auto tp = Graphics::Texture2D::Create(Graphics::Image::Create(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "placeholders/missing_texture32.png"));
		RegisterPlaceholder<Graphics::Texture2D>(tp);
	}
}
