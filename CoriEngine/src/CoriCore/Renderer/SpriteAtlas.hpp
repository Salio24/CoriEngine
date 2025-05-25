#pragma once
#include "../AssetManager/SpriteAtlasDescriptor.hpp"
#include "Sprite.hpp"
#include "Texture.hpp"

namespace Cori {

	class SpriteAtlas {
	public:

		static std::shared_ptr<SpriteAtlas> Create(const SpriteAtlasDescriptor& descriptor);

		const std::shared_ptr<Texture2D> GetTexture() const;

		const UVs& GetSpriteUVsAtIndex(uint32_t index) const;

		const UVs& GetSpriteUVsAtPosition(glm::ivec2 pos) const;

	private:

		SpriteAtlas(const SpriteAtlasDescriptor& descriptor);

		std::string_view m_DebugName;

		// both referring to grind dimension and grid size, not to pixel ones
		glm::ivec2 m_SpriteAtlasDimensions;
		uint32_t m_SpriteAtlasSize;

		const Texture2DDescriptor& m_TextureDescriptor;

		std::vector<UVs> m_SpriteUVs;
	};
}