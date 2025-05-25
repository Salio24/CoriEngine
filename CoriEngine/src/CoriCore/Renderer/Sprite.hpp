#pragma once
#include "Texture.hpp"

namespace Cori {
	class Sprite {
	public:
		static std::shared_ptr<Sprite> Create(const std::shared_ptr<Texture2D>& texture, const UVs& uv);

		const std::shared_ptr<Texture2D> GetTexture() const { return m_Texture; }
		const UVs GetUVs() const { return m_UVs; }

	private:
		Sprite(const std::shared_ptr<Texture2D>& texture, const UVs& uv);

		std::shared_ptr<Texture2D> m_Texture;
		UVs m_UVs;
	};
}