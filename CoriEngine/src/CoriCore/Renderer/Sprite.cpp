#include "Sprite.hpp"
#include "../AssetManager/AssetManager.hpp"

namespace Cori {
	bool Sprite::PreCreateHook(const std::shared_ptr<Texture2D>& texture, const UVs& uv) {

		return true;
	}

	Sprite::Sprite(const std::shared_ptr<Texture2D>& texture, const UVs& uv) : m_Texture(texture), m_UVs(uv) {

	}


}