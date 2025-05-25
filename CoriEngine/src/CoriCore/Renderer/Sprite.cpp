#include "Sprite.hpp"
#include "../AssetManager/AssetManager.hpp"

namespace Cori {
	std::shared_ptr<Sprite> Sprite::Create(const std::shared_ptr<Texture2D>& texture, const UVs& uv) {
		struct EnableShared : public Sprite {
			EnableShared(const std::shared_ptr<Texture2D>& texture, const UVs& uv) : Sprite(texture, uv) { }
		};

		return std::make_shared<EnableShared>(texture, uv);
	}

	Sprite::Sprite(const std::shared_ptr<Texture2D>& texture, const UVs& uv) : m_Texture(texture), m_UVs(uv) { }
}