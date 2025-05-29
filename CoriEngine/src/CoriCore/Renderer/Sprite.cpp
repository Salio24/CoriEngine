#include "Sprite.hpp"
#include "../AssetManager/AssetManager.hpp"

namespace Cori {

	CORI_DEFINE_SHARED_SELF_FACTORY(
		Sprite,
			{

			},
			(const std::shared_ptr<Texture2D>& texture, const UVs& uv),
				: m_Texture(texture), m_UVs(uv) {

			}
		);

}