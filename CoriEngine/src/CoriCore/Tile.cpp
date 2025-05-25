#include "Tile.hpp"
#include "AssetManager/AssetManager.hpp"

namespace Cori {
	std::shared_ptr<Tile> Tile::Create(const glm::vec2 position, const glm::vec2 size, const SpriteAtlasDescriptor& descriptor, const uint32_t index) {
		struct EnableShared : public Tile {
			EnableShared(const glm::vec2 position, const glm::vec2 size, const SpriteAtlasDescriptor& descriptor, const uint32_t index) : Tile(position, size, descriptor, index) {}
		};
		return std::make_shared<EnableShared>(position, size, descriptor, index);
	}

	Tile::Tile(const glm::vec2 position, const glm::vec2 size, const SpriteAtlasDescriptor& descriptor, const uint32_t index) : m_Position(position), m_Size(size) {
		std::shared_ptr<SpriteAtlas> atlas = AssetManager::GetSpriteAtlas(descriptor);
		m_Sprite = Sprite::Create(atlas->GetTexture(), atlas->GetSpriteUVsAtIndex(index));
	}
}
