#pragma once
#include "Texture.hpp"
#include "../Profiling/Trackable.hpp"
#include "../SelfFactory.hpp"

namespace Cori {
	class Sprite : public Profiling::Trackable<Sprite>, public SharedSeflFactory<Sprite> {
		CORI_DECLARE_SHARED_SELF_FACTORY(Sprite, (const std::shared_ptr<Texture2D>& texture, const UVs& uv));

	public:

		const std::shared_ptr<Texture2D> GetTexture() const { return m_Texture; }
		const UVs GetUVs() const { return m_UVs; }

	private:

		std::shared_ptr<Texture2D> m_Texture;
		UVs m_UVs;
	};

}